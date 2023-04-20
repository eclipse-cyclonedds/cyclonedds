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
#include "ac_tokens.h"

static const char *RELATIVE_PATH_TO_ETC_DIR = "/get_permissions_token/etc/";

static const char *IDENTITY_CERTIFICATE =
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

static const char *IDENTITY_CA =
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

static const char *PRIVATE_KEY =
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

static const char *PERMISSIONS_CA =
    "data:,-----BEGIN CERTIFICATE-----\n"
    "MIIDjTCCAnWgAwIBAgIJANsr3sm0NrypMA0GCSqGSIb3DQEBCwUAMFwxCzAJBgNV\n"
    "BAYTAk5MMRMwEQYDVQQIDApTb21lLVN0YXRlMR8wHQYDVQQKDBZBRExJTksgVGVj\n"
    "aG5vbG9jeSBJbmMuMRcwFQYDVQQDDA5hZGxpbmt0ZWNoLmNvbTAgFw0xODA3MzAx\n"
    "MjQ1NTVaGA8yMTE4MDcwNjEyNDU1NVowXDELMAkGA1UEBhMCTkwxEzARBgNVBAgM\n"
    "ClNvbWUtU3RhdGUxHzAdBgNVBAoMFkFETElOSyBUZWNobm9sb2N5IEluYy4xFzAV\n"
    "BgNVBAMMDmFkbGlua3RlY2guY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB\n"
    "CgKCAQEAu7jfnJ0wYVuXgG+PgNawdN38+dRpa8jceqi+blIDehV6XCxrnGXusTCD\n"
    "uFmo7HMOBVMVNDXlcBWgoGd+u5EultnOEiIeGTgtHc1O6V9wicp3BGSpZZax/TcO\n"
    "NjMVORaqHCADbQ2J8wsz1FHxuKDwX6BJElYOlK77lb/x3yLsDFFC+a0qn2RFh37r\n"
    "cWBRAHy8VEASXKZElT9ZmfKd+KUq34KojhNJ4DepKStTq074BRDXVivx+wVD951L\n"
    "FNPiQXq+mgHcLj1k37KlZflTFhdP5oEMtATNsXNJPHlEymiySogRWAmKhysLQudu\n"
    "kHfNKN+r0FEQMk/hzpYcFeZSOvbfNQIDAQABo1AwTjAdBgNVHQ4EFgQURWMbWvBK\n"
    "ZwJvRV1/tyc1R82k0+gwHwYDVR0jBBgwFoAURWMbWvBKZwJvRV1/tyc1R82k0+gw\n"
    "DAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAkPF+ysVtvHnk2hpu9yND\n"
    "LCJ96ZzIoKOyY7uRj4ovzlAHFdpNOJQdcJihTmN8i7Trht9XVh0rGoR/6nHzo3TI\n"
    "eiogRC80RlDtuA3PF2dDQBMVDStlZMTZPb693hfjdAjhyyw9yghhKHHqNDvSsAL0\n"
    "KfBqjG4yGfGpJylYXIT5fWuKlo/ln/yyPa5s54T5XDo+CMbtlLX3QnwVOmaRyzyl\n"
    "PiTcPCDIkdLBdXmlfyJcmW6fWa6kPx+35MOxPsXZbujCo+42+OyLqcH1rKT6Xhcs\n"
    "hjXBEf+kdgUfSClrM1pNRWsw2ChIYim0F+nry5JFy0Y+8Hbb6SDB340BFmtgDHbF\n"
    "HQ==\n"
    "-----END CERTIFICATE-----\n";

static char *g_path_to_etc_dir = NULL;
static struct plugins_hdl *plugins = NULL;
static dds_security_authentication *auth = NULL;
static dds_security_access_control *access_control = NULL;

static DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_PermissionsHandle local_permissions_handle = DDS_SECURITY_HANDLE_NIL;

static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
}

static DDS_Security_Property_t *find_property(DDS_Security_DataHolder *token, const char *name)
{
  DDS_Security_Property_t *result = NULL;
  uint32_t i;
  for (i = 0; i < token->properties._length && !result; i++)
    if (token->properties._buffer[i].name && (strcmp(token->properties._buffer[i].name, name) == 0))
      result = &token->properties._buffer[i];
  return result;
}

static void dds_security_property_init(DDS_Security_PropertySeq *seq, DDS_Security_unsigned_long size)
{
  seq->_length = size;
  seq->_maximum = size;
  seq->_buffer = ddsrt_malloc(size * sizeof(DDS_Security_Property_t));
  memset(seq->_buffer, 0, size * sizeof(DDS_Security_Property_t));
}

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

static void fill_participant_qos(DDS_Security_Qos *qos, const char *permission_filename, const char *governance_filename)
{
  char *permission_uri;
  char *governance_uri;

  ddsrt_asprintf(&permission_uri, "file:%s%s", g_path_to_etc_dir, permission_filename);
  ddsrt_asprintf(&governance_uri, "file:%s%s", g_path_to_etc_dir, governance_filename);

  memset(qos, 0, sizeof(*qos));
  dds_security_property_init(&qos->property.value, 6);
  qos->property.value._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
  qos->property.value._buffer[0].value = ddsrt_strdup(IDENTITY_CERTIFICATE);
  qos->property.value._buffer[1].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
  qos->property.value._buffer[1].value = ddsrt_strdup(IDENTITY_CA);
  qos->property.value._buffer[2].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
  qos->property.value._buffer[2].value = ddsrt_strdup(PRIVATE_KEY);
  qos->property.value._buffer[3].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_PERMISSIONS_CA);
  qos->property.value._buffer[3].value = ddsrt_strdup(PERMISSIONS_CA);
  qos->property.value._buffer[4].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_PERMISSIONS);
  qos->property.value._buffer[4].value = ddsrt_strdup(permission_uri);
  qos->property.value._buffer[5].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_GOVERNANCE);
  qos->property.value._buffer[5].value = ddsrt_strdup(governance_uri);

  ddsrt_free(permission_uri);
  ddsrt_free(governance_uri);
}

static void local_permissions_init(DDS_Security_DomainId domain_id)
{
  DDS_Security_ValidationResult_t result;
  DDS_Security_Qos participant_qos;
  DDS_Security_GUID_t local_participant_guid;
  DDS_Security_GUID_t candidate_participant_guid;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_GuidPrefix_t prefix = {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb};
  DDS_Security_EntityId_t entityId = {{0xb0, 0xb1, 0xb2}, 0x1};

  memset(&local_participant_guid, 0, sizeof(local_participant_guid));
  memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
  memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

  fill_participant_qos(&participant_qos, "Test_Permissions_ok.p7s", "Test_Governance_ok.p7s");

  result = auth->validate_local_identity(
      auth,
      &local_identity_handle,
      &local_participant_guid,
      domain_id,
      &participant_qos,
      &candidate_participant_guid,
      &exception);

  CU_ASSERT_EQUAL_FATAL (result, DDS_SECURITY_VALIDATION_OK);
  reset_exception(&exception);
  local_permissions_handle = access_control->validate_local_permissions(
      access_control,
      auth,
      local_identity_handle,
      domain_id,
      &participant_qos,
      &exception);

  CU_ASSERT_FATAL (local_permissions_handle != DDS_SECURITY_HANDLE_NIL);
  reset_exception(&exception);
  dds_security_property_deinit(&participant_qos.property.value);
}

static void local_permissions_clean(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_boolean success;

  if (local_permissions_handle != DDS_SECURITY_HANDLE_NIL)
  {
    success = access_control->return_permissions_handle(access_control, local_permissions_handle, &exception);
    if (!success)
    {
      printf("return_permission_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
  }

  if (local_identity_handle != DDS_SECURITY_HANDLE_NIL)
  {
    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    if (!success)
    {
      printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
  }
}

static void set_path_to_etc_dir(void)
{
  ddsrt_asprintf(&g_path_to_etc_dir, "%s%s", CONFIG_ENV_TESTS_DIR, RELATIVE_PATH_TO_ETC_DIR);
}

static void suite_get_permissions_token_init(void)
{
  plugins = load_plugins(&access_control, &auth, NULL /* Cryptograpy */, NULL);
  CU_ASSERT_FATAL (plugins != NULL);
  set_path_to_etc_dir();
  local_permissions_init(0);
}

static void suite_get_permissions_token_fini(void)
{
  local_permissions_clean();
  unload_plugins(plugins);
  ddsrt_free(g_path_to_etc_dir);
}

static bool validate_permissions_token(
    DDS_Security_PermissionsToken *token)
{
  if (!token->class_id || strcmp(token->class_id, DDS_ACTOKEN_PERMISSIONS_CLASS_ID) != 0)
  {
    CU_FAIL("PermissionsToken incorrect class_id");
    return false;
  }

  /* Optional. */
  if (find_property(token, DDS_ACTOKEN_PROP_PERM_CA_SN) == NULL)
    printf("Optional PermissionsToken property '" DDS_ACTOKEN_PROP_PERM_CA_SN "' not found\n");
  if (find_property(token, DDS_ACTOKEN_PROP_PERM_CA_ALGO) == NULL)
    printf("Optional PermissionsToken property '" DDS_ACTOKEN_PROP_PERM_CA_ALGO "' not found\n");
  return true;
}

CU_Test(ddssec_builtin_get_permissions_token, happy_day, .init = suite_get_permissions_token_init, .fini = suite_get_permissions_token_fini)
{
  DDS_Security_SecurityException exception;
  DDS_Security_PermissionsToken token;
  DDS_Security_boolean result;

  /* Pre-requisites. */
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_permissions_token != NULL);
  assert(access_control->get_permissions_token != 0);
  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&token, 0, sizeof(token));

  /* Test function call. */
  result = access_control->get_permissions_token(
      access_control,
      &token,
      local_permissions_handle,
      &exception);
  if (!result)
  {
    printf("get_permissions_token: %s\n", exception.message ? exception.message : "Error message missing");
  }
  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  /* Test token contents. */
  CU_ASSERT(validate_permissions_token(&token));

  /* Post-requisites. */
  DDS_Security_DataHolder_deinit(&token);
  reset_exception(&exception);
}

CU_Test(ddssec_builtin_get_permissions_token, invalid_args, .init = suite_get_permissions_token_init, .fini = suite_get_permissions_token_fini)
{
  DDS_Security_SecurityException exception;
  DDS_Security_PermissionsToken token;
  DDS_Security_boolean result;

  /* Pre-requisites. */
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_permissions_token != NULL);
  assert(access_control->get_permissions_token != 0);
  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&token, 0, sizeof(token));

  /* Test function calls with different invalid args. */
  result = access_control->get_permissions_token(
      NULL,
      &token,
      local_permissions_handle,
      &exception);
  if (!result)
  {
    printf("get_permissions_token: %s\n", exception.message ? exception.message : "Error message missing");
  }
  CU_ASSERT(!result);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  result = access_control->get_permissions_token(
      access_control,
      NULL,
      local_permissions_handle,
      &exception);
  if (!result)
  {
    printf("get_permissions_token: %s\n", exception.message ? exception.message : "Error message missing");
  }
  CU_ASSERT(!result);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  result = access_control->get_permissions_token(
      access_control,
      &token,
      0,
      &exception);
  if (!result)
  {
    printf("get_permissions_token: %s\n", exception.message ? exception.message : "Error message missing");
  }
  CU_ASSERT(!result);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  result = access_control->get_permissions_token(
      access_control,
      &token,
      local_permissions_handle,
      NULL);
  if (!result)
  {
    printf("get_permissions_token: %s\n", exception.message ? exception.message : "Error message missing");
  }
  CU_ASSERT(!result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);
  reset_exception(&exception);

  result = access_control->get_permissions_token(
      access_control,
      &token,
      local_permissions_handle + 12345 /* invalid handle */,
      &exception);
  if (!result)
  {
    printf("get_permissions_token: %s\n", exception.message ? exception.message : "Error message missing");
  }
  CU_ASSERT(!result);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);
}
