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

#if OPENSLL_VERSION_NUMBER >= 0x10002000L
#define AUTH_INCLUDE_EC
#endif

static const char *RELATIVE_PATH_TO_ETC_DIR = "/get_xxx_sec_attributes/etc/";

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

static struct plugins_hdl *plugins = NULL;
static dds_security_access_control *access_control = NULL;
static dds_security_authentication *auth = NULL;
static DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_PermissionsHandle local_permissions_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_GUID_t local_participant_guid;
static char *g_path_to_etc_dir = NULL;

typedef enum SEC_TOPIC_NAME
{
  SEC_TOPIC_DCPSPARTICIPANTSECURE,
  SEC_TOPIC_DCPSPUBLICATIONSSECURE,
  SEC_TOPIC_DCPSSUBSCRIPTIONSSECURE,
  SEC_TOPIC_DCPSPARTICIPANTMESSAGESECURE,
  SEC_TOPIC_DCPSPARTICIPANTSTATELESSMESSAGE,
  SEC_TOPIC_DCPSPARTICIPANTVOLATILEMESSAGESECURE,
  SEC_TOPIC_DCPS_KINEMATICS,
  SEC_TOPIC_DCPS_OWNSHIPDATA,
  SEC_TOPIC_DCPS_SHAPE
} SEC_TOPIC_TYPE;

const char *TOPIC_NAMES[] = {"DCPSParticipantsSecure",
                             "DCPSPublicationsSecure",
                             "DCPSSubscriptionsSecure",
                             "DCPSParticipantMessageSecure",
                             "DCPSParticipantStatelessMessage",
                             "DCPSParticipantVolatileMessageSecure",
                             "Kinematics",
                             "OwnShipData",
                             "Shape"

};

static DDS_Security_EndpointSecurityAttributes ATTRIBUTE_CHECKLIST[9];

static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
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

static void fill_participant_qos(DDS_Security_Qos *qos, const char *permission_filename,
                                 const char *governance_filename)
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

static bool create_local_identity(DDS_Security_DomainId domain_id, const char *governance_file)
{
  DDS_Security_ValidationResult_t result;
  DDS_Security_Qos participant_qos;
  DDS_Security_GUID_t candidate_participant_guid;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_GuidPrefix_t prefix = {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb};
  DDS_Security_EntityId_t entityId = {{0xb0, 0xb1, 0xb2}, 0x1};

  memset(&local_participant_guid, 0, sizeof(local_participant_guid));
  memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
  memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

  fill_participant_qos(&participant_qos, "Test_Permissions_ok.p7s", governance_file);

  /* Now call the function. */
  result = auth->validate_local_identity(
      auth,
      &local_identity_handle,
      &local_participant_guid,
      domain_id,
      &participant_qos,
      &candidate_participant_guid,
      &exception);

  if (result != DDS_SECURITY_VALIDATION_OK)
  {
    printf("[ERROR] validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    return false;
  }

  local_permissions_handle = access_control->validate_local_permissions(
      access_control,
      auth,
      local_identity_handle,
      domain_id,
      &participant_qos,
      &exception);

  if (local_permissions_handle == DDS_SECURITY_HANDLE_NIL)
  {
    printf("[ERROR] validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    return false;
  }

  dds_security_property_deinit(&participant_qos.property.value);

  return true;
}

static void clear_local_identity(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_boolean success;

  if (local_identity_handle != DDS_SECURITY_HANDLE_NIL)
  {
    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    if (!success)
    {
      printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
  }

  if (local_permissions_handle != DDS_SECURITY_HANDLE_NIL)
  {
    success = access_control->return_permissions_handle(access_control, local_permissions_handle, &exception);
    if (!success)
    {
      printf("return_permissions_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
  }

  local_identity_handle = DDS_SECURITY_HANDLE_NIL;
  local_permissions_handle = DDS_SECURITY_HANDLE_NIL;
}

static void set_path_to_etc_dir(void)
{
  ddsrt_asprintf(&g_path_to_etc_dir, "%s%s", CONFIG_ENV_TESTS_DIR, RELATIVE_PATH_TO_ETC_DIR);
}

static DDS_Security_PluginEndpointSecurityAttributesMask get_plugin_endpoint_security_attributes_mask(DDS_Security_boolean is_payload_encrypted, DDS_Security_boolean is_submessage_encrypted, DDS_Security_boolean is_submessage_origin_authenticated)
{
  DDS_Security_PluginEndpointSecurityAttributesMask mask = DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_VALID;
  if (is_submessage_encrypted)
    mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
  if (is_payload_encrypted)
    mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED;
  if (is_submessage_origin_authenticated)
    mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
  return mask;
}

static void suite_get_xxx_sec_attributes_init(void)
{
  set_path_to_etc_dir();

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSECURE].is_read_protected =
      ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPUBLICATIONSSECURE].is_read_protected =
          ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSSUBSCRIPTIONSSECURE].is_read_protected =
              ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTMESSAGESECURE].is_read_protected =
                  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSTATELESSMESSAGE].is_read_protected =
                      ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTVOLATILEMESSAGESECURE].is_read_protected = false;

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSECURE].is_write_protected =
      ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPUBLICATIONSSECURE].is_write_protected =
          ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSSUBSCRIPTIONSSECURE].is_write_protected =
              ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTMESSAGESECURE].is_write_protected =
                  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSTATELESSMESSAGE].is_write_protected =
                      ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTVOLATILEMESSAGESECURE].is_write_protected = false;

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSECURE].is_payload_protected =
      ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPUBLICATIONSSECURE].is_payload_protected =
          ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSSUBSCRIPTIONSSECURE].is_payload_protected =
              ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTMESSAGESECURE].is_payload_protected =
                  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSTATELESSMESSAGE].is_payload_protected =
                      ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTVOLATILEMESSAGESECURE].is_payload_protected = false;

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSECURE].is_key_protected =
      ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPUBLICATIONSSECURE].is_key_protected =
          ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSSUBSCRIPTIONSSECURE].is_key_protected =
              ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTMESSAGESECURE].is_key_protected =
                  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSTATELESSMESSAGE].is_key_protected =
                      ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTVOLATILEMESSAGESECURE].is_key_protected = false;

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSECURE].is_submessage_protected =
      ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPUBLICATIONSSECURE].is_submessage_protected =
          ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSSUBSCRIPTIONSSECURE].is_submessage_protected =
              ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTMESSAGESECURE].is_submessage_protected = true;

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTSTATELESSMESSAGE].is_submessage_protected = false;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPSPARTICIPANTVOLATILEMESSAGESECURE].is_submessage_protected = true;

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_KINEMATICS].is_read_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_KINEMATICS].is_write_protected = false;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_KINEMATICS].is_discovery_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_KINEMATICS].is_liveliness_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_KINEMATICS].is_submessage_protected = false;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_KINEMATICS].is_payload_protected = false;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_KINEMATICS].is_key_protected = false;

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_KINEMATICS].plugin_endpoint_attributes =
      get_plugin_endpoint_security_attributes_mask(false, false, false);

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_OWNSHIPDATA].is_read_protected = false;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_OWNSHIPDATA].is_write_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_OWNSHIPDATA].is_discovery_protected = false;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_OWNSHIPDATA].is_liveliness_protected = false;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_OWNSHIPDATA].is_submessage_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_OWNSHIPDATA].is_payload_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_OWNSHIPDATA].is_key_protected = true;

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_OWNSHIPDATA].plugin_endpoint_attributes =
      get_plugin_endpoint_security_attributes_mask(true, false, false);

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_SHAPE].is_read_protected = false;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_SHAPE].is_write_protected = false;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_SHAPE].is_discovery_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_SHAPE].is_liveliness_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_SHAPE].is_submessage_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_SHAPE].is_payload_protected = true;
  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_SHAPE].is_key_protected = true;

  ATTRIBUTE_CHECKLIST[SEC_TOPIC_DCPS_SHAPE].plugin_endpoint_attributes =
      get_plugin_endpoint_security_attributes_mask(true, true, true);
}

static void suite_get_xxx_sec_attributes_fini(void)
{
  ddsrt_free(g_path_to_etc_dir);
}

static bool plugins_init(void)
{
  /* Checking AccessControl, but needing Authentication to setup local identity. */
  plugins = load_plugins(&access_control, &auth, NULL /* Cryptograpy */, NULL);
  return plugins ? true : false;
}

static void plugins_fini(void)
{
  unload_plugins(plugins);
}

static bool
verify_endpoint_attributes(SEC_TOPIC_TYPE topic_type, DDS_Security_EndpointSecurityAttributes *attributes)
{
  bool result = true;
  if (attributes->is_read_protected != ATTRIBUTE_CHECKLIST[topic_type].is_read_protected ||
      attributes->is_write_protected != ATTRIBUTE_CHECKLIST[topic_type].is_write_protected ||
      attributes->is_submessage_protected != ATTRIBUTE_CHECKLIST[topic_type].is_submessage_protected ||
      attributes->is_payload_protected != ATTRIBUTE_CHECKLIST[topic_type].is_payload_protected ||
      attributes->is_key_protected != ATTRIBUTE_CHECKLIST[topic_type].is_key_protected)
  {

    result = false;
  }
  if (topic_type == SEC_TOPIC_DCPS_KINEMATICS || topic_type == SEC_TOPIC_DCPS_SHAPE)
  {
    if (attributes->is_discovery_protected != ATTRIBUTE_CHECKLIST[topic_type].is_discovery_protected ||
        attributes->is_liveliness_protected != ATTRIBUTE_CHECKLIST[topic_type].is_liveliness_protected ||
        attributes->plugin_endpoint_attributes != ATTRIBUTE_CHECKLIST[topic_type].plugin_endpoint_attributes)
    {
      result = false;
    }
  }

  if (!result)
  {
    printf("Invalid attribute for Topic: %s\n", TOPIC_NAMES[topic_type]);
    printf("is_read_protected: EXPECTED: %u ACTUAL: %u\n"
           "is_write_protected: EXPECTED: %u ACTUAL: %u\n"
           "is_discovery_protected: EXPECTED: %u ACTUAL: %u\n"
           "is_liveliness_protected: EXPECTED: %u ACTUAL: %u\n"
           "is_submessage_protected: EXPECTED: %u ACTUAL: %u\n"
           "is_payload_protected: EXPECTED: %u ACTUAL: %u\n"
           "is_key_protected: EXPECTED: %u ACTUAL: %u\n",
           ATTRIBUTE_CHECKLIST[topic_type].is_read_protected, attributes->is_read_protected,
           ATTRIBUTE_CHECKLIST[topic_type].is_write_protected, attributes->is_write_protected,
           ATTRIBUTE_CHECKLIST[topic_type].is_discovery_protected, attributes->is_discovery_protected,
           ATTRIBUTE_CHECKLIST[topic_type].is_liveliness_protected, attributes->is_liveliness_protected,
           ATTRIBUTE_CHECKLIST[topic_type].is_submessage_protected, attributes->is_submessage_protected,
           ATTRIBUTE_CHECKLIST[topic_type].is_payload_protected, attributes->is_payload_protected,
           ATTRIBUTE_CHECKLIST[topic_type].is_key_protected, attributes->is_key_protected);
  }

  return result;
}

static bool verify_topic_attributes(SEC_TOPIC_TYPE topic_type, DDS_Security_TopicSecurityAttributes *attributes)
{
  bool result = true;
  if (attributes->is_read_protected != ATTRIBUTE_CHECKLIST[topic_type].is_read_protected ||
      attributes->is_write_protected != ATTRIBUTE_CHECKLIST[topic_type].is_write_protected ||
      attributes->is_discovery_protected != ATTRIBUTE_CHECKLIST[topic_type].is_discovery_protected ||
      attributes->is_liveliness_protected != ATTRIBUTE_CHECKLIST[topic_type].is_liveliness_protected)
  {
    result = false;
  }

  if (!result)
  {
    printf("Invalid attribute for Topic: %s\n", TOPIC_NAMES[topic_type]);
    printf("is_read_protected: EXPECTED: %u ACTUAL: %u\n"
           "is_write_protected: EXPECTED: %u ACTUAL: %u\n"
           "is_discovery_protected: EXPECTED: %u ACTUAL: %u\n"
           "is_liveliness_protected: EXPECTED: %u ACTUAL: %u\n",
           ATTRIBUTE_CHECKLIST[topic_type].is_read_protected, attributes->is_read_protected,
           ATTRIBUTE_CHECKLIST[topic_type].is_write_protected, attributes->is_write_protected,
           ATTRIBUTE_CHECKLIST[topic_type].is_discovery_protected, attributes->is_discovery_protected,
           ATTRIBUTE_CHECKLIST[topic_type].is_liveliness_protected, attributes->is_liveliness_protected);
  }

  return result;
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, participant_happy_day, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_ParticipantSecurityAttributes attributes;
  bool result;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_participant_sec_attributes != NULL);
  assert(access_control->get_participant_sec_attributes != 0);

  result = create_local_identity(0, "Test_Governance_full.p7s");
  CU_ASSERT_FATAL(result);

  memset(&attributes, 0, sizeof(attributes));

  result = access_control->get_participant_sec_attributes(
      access_control,
      local_permissions_handle,
      &attributes,
      &exception);
  CU_ASSERT(result);

  /*
     * Expect these values based on these options, which is the 1st domain rule
     * in the Test_Governance_full.p7s (selected because of domain id 0):
     *
     *    <allow_unauthenticated_participants>false</allow_unauthenticated_participants>
     *    <enable_join_access_control>true</enable_join_access_control>
     *    <discovery_protection_kind>SIGN_WITH_ORIGIN_AUTHENTICATION</discovery_protection_kind>
     *    <liveliness_protection_kind>ENCRYPT</liveliness_protection_kind>
     *    <rtps_protection_kind>ENCRYPT_WITH_ORIGIN_AUTHENTICATION</rtps_protection_kind>
     */
  CU_ASSERT(attributes.allow_unauthenticated_participants == false);
  CU_ASSERT(attributes.is_access_protected == true);
  CU_ASSERT(attributes.is_discovery_protected == true);
  CU_ASSERT(attributes.is_liveliness_protected == true);
  CU_ASSERT(attributes.is_rtps_protected == true);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED) == DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED) == 0);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED) == DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED) == DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED) == DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED) == 0);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID) != 0);

  result = access_control->return_participant_sec_attributes(
      access_control,
      &attributes,
      &exception);
  CU_ASSERT(result);

  clear_local_identity();
  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, datawriter_happy_day, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PartitionQosPolicy *partition = NULL;
  DDS_Security_DataTagQosPolicy data_tag;
  DDS_Security_EndpointSecurityAttributes attributes;
  bool result;
  unsigned i;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_datawriter_sec_attributes != NULL);
  assert(access_control->get_datawriter_sec_attributes != 0);

  result = create_local_identity(0, "Test_Governance_full.p7s");
  CU_ASSERT_FATAL(result);

  memset(&attributes, 0, sizeof(attributes));

  /*Test for each builtin topics:
     "DCPSParticipantsSecure", "DCPSPublicationsSecure", "DCPSSubscriptionsSecure"
     "DCPSParticipantMessageSecure", "DCPSParticipantStatelessMessage", "DCPSParticipantVolatileMessageSecure"
    and a sample DCPS topic*/

  /* Now call the function. */
  for (i = SEC_TOPIC_DCPSPARTICIPANTSECURE; i <= SEC_TOPIC_DCPS_SHAPE; ++i)
  {

    result = access_control->get_datawriter_sec_attributes(
        access_control,
        local_permissions_handle,
        TOPIC_NAMES[i],
        partition,
        &data_tag,
        &attributes,
        &exception);

    CU_ASSERT_FATAL(result);

    CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_OK_CODE);
    CU_ASSERT_FATAL(verify_endpoint_attributes(i, &attributes));

    //reset control values
    memset(&attributes, 0, sizeof(attributes));
    reset_exception(&exception);
  }

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, datawriter_non_existing_topic, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PartitionQosPolicy *partition = NULL;
  DDS_Security_DataTagQosPolicy data_tag;
  DDS_Security_EndpointSecurityAttributes attributes;
  bool result;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_datawriter_sec_attributes != NULL);
  assert(access_control->get_datawriter_sec_attributes != 0);

  /* use a different domain(30) to get non matching topic result */
  result = create_local_identity(30, "Test_Governance_full.p7s");
  CU_ASSERT_FATAL(result);

  memset(&attributes, 0, sizeof(attributes));

  /* Now call the function. */
  result = access_control->get_datawriter_sec_attributes(
      access_control,
      local_permissions_handle,
      TOPIC_NAMES[SEC_TOPIC_DCPS_SHAPE],
      partition,
      &data_tag,
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);

  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_CAN_NOT_FIND_TOPIC_IN_DOMAIN_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, datareader_happy_day, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PartitionQosPolicy *partition = NULL;
  DDS_Security_DataTagQosPolicy data_tag;
  DDS_Security_EndpointSecurityAttributes attributes;
  bool result;
  unsigned i;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_datareader_sec_attributes != NULL);
  assert(access_control->get_datareader_sec_attributes != 0);

  result = create_local_identity(0, "Test_Governance_full.p7s");
  CU_ASSERT_FATAL(result);

  memset(&attributes, 0, sizeof(attributes));

  /*Test for each builtin topics:
     "DCPSParticipantSecure", "DCPSPublicationsSecure", "DCPSSubscriptionsSecure"
     "DCPSParticipantMessageSecure", "DCPSParticipantStatelessMessage", "DCPSParticipantVolatileMessageSecure"
     and a sample DCPS topic*/

  /* Now call the function. */
  for (i = SEC_TOPIC_DCPSPARTICIPANTSECURE; i <= SEC_TOPIC_DCPS_SHAPE; ++i)
  {

    result = access_control->get_datareader_sec_attributes(
        access_control,
        local_permissions_handle,
        TOPIC_NAMES[i],
        partition,
        &data_tag,
        &attributes,
        &exception);

    CU_ASSERT_FATAL(result);

    CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_OK_CODE);
    CU_ASSERT_FATAL(verify_endpoint_attributes(i, &attributes) == true);

    //reset control values
    memset(&attributes, 0, sizeof(attributes));
    reset_exception(&exception);
  }

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, datareader_non_existing_topic, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PartitionQosPolicy *partition = NULL;
  DDS_Security_DataTagQosPolicy data_tag;
  DDS_Security_EndpointSecurityAttributes attributes;
  bool result;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_datawriter_sec_attributes != NULL);
  assert(access_control->get_datawriter_sec_attributes != 0);

  /* use a different domain (30) to get non matching topic result */
  result = create_local_identity(30, "Test_Governance_full.p7s");
  CU_ASSERT_FATAL(result);

  memset(&attributes, 0, sizeof(attributes));

  result = access_control->get_datareader_sec_attributes(
      access_control,
      local_permissions_handle,
      TOPIC_NAMES[SEC_TOPIC_DCPS_SHAPE],
      partition,
      &data_tag,
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);

  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_CAN_NOT_FIND_TOPIC_IN_DOMAIN_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, participant_invalid_param, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_ParticipantSecurityAttributes attributes;
  bool result;

  memset(&attributes, 0, sizeof(attributes));

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_participant_sec_attributes != NULL);
  assert(access_control->get_participant_sec_attributes != 0);

  result = access_control->get_participant_sec_attributes(
      NULL,
      local_permissions_handle,
      &attributes,
      &exception);
  CU_ASSERT(!result);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_participant_sec_attributes(
      access_control,
      0,
      &attributes,
      &exception);
  CU_ASSERT(!result);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_participant_sec_attributes(
      access_control,
      local_permissions_handle,
      NULL,
      &exception);
  CU_ASSERT(!result);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_participant_sec_attributes(
      access_control,
      local_permissions_handle + 12345,
      &attributes,
      &exception);
  CU_ASSERT(!result);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, datareader_invalid_param, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PartitionQosPolicy *partition = NULL;
  DDS_Security_DataTagQosPolicy data_tag;
  DDS_Security_EndpointSecurityAttributes attributes;
  bool result;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_datareader_sec_attributes != NULL);
  assert(access_control->get_datareader_sec_attributes != 0);

  memset(&attributes, 0, sizeof(attributes));

  /* Now call the function. */

  result = access_control->get_datareader_sec_attributes(
      access_control,
      local_permissions_handle,
      NULL,
      partition,
      &data_tag,
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_datareader_sec_attributes(
      access_control,
      local_permissions_handle,
      "",
      partition,
      &data_tag,
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_datareader_sec_attributes(
      access_control,
      local_permissions_handle,
      "Shape",
      partition,
      &data_tag,
      NULL,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_datareader_sec_attributes(
      access_control,
      local_permissions_handle + 12345,
      "Shape",
      partition,
      &data_tag,
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, datawriter_invalid_param, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PartitionQosPolicy *partition = NULL;
  DDS_Security_DataTagQosPolicy data_tag;
  DDS_Security_EndpointSecurityAttributes attributes;
  bool result;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_datawriter_sec_attributes != NULL);
  assert(access_control->get_datawriter_sec_attributes != 0);

  memset(&attributes, 0, sizeof(attributes));

  /* Now call the function. */

  result = access_control->get_datawriter_sec_attributes(
      access_control,
      local_permissions_handle,
      NULL,
      partition,
      &data_tag,
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_datawriter_sec_attributes(
      access_control,
      local_permissions_handle,
      "",
      partition,
      &data_tag,
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_datawriter_sec_attributes(
      access_control,
      local_permissions_handle,
      "Shape",
      partition,
      &data_tag,
      NULL,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_datawriter_sec_attributes(
      access_control,
      local_permissions_handle + 12345,
      "Shape",
      partition,
      &data_tag,
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, topic_happy_day, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_TopicSecurityAttributes attributes;
  bool result;
  unsigned i;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_topic_sec_attributes != NULL);
  assert(access_control->get_topic_sec_attributes != 0);

  result = create_local_identity(0, "Test_Governance_full.p7s");
  CU_ASSERT_FATAL(result);

  memset(&attributes, 0, sizeof(attributes));

  /*Test for each builtin topics:
     "DCPSParticipantsSecure", "DCPSPublicationsSecure", "DCPSSubscriptionsSecure"
     "DCPSParticipantMessageSecure", "DCPSParticipantStatelessMessage", "DCPSParticipantVolatileMessageSecure"
    and a sample DCPS topic*/

  /* Now call the function. */
  for (i = SEC_TOPIC_DCPS_KINEMATICS; i <= SEC_TOPIC_DCPS_SHAPE; ++i)
  {

    result = access_control->get_topic_sec_attributes(
        access_control,
        local_permissions_handle,
        TOPIC_NAMES[i],
        &attributes,
        &exception);

    CU_ASSERT_FATAL(result);

    CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_OK_CODE);
    CU_ASSERT_FATAL(verify_topic_attributes(i, &attributes));

    //reset control values
    memset(&attributes, 0, sizeof(attributes));
    reset_exception(&exception);
  }

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, topic_non_existing_topic, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_TopicSecurityAttributes attributes;
  bool result;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_topic_sec_attributes != NULL);
  assert(access_control->get_topic_sec_attributes != 0);

  result = create_local_identity(30, "Test_Governance_full.p7s");
  CU_ASSERT_FATAL(result);

  memset(&attributes, 0, sizeof(attributes));

  /*Test for each builtin topics:
     "DCPSParticipantsSecure", "DCPSPublicationsSecure", "DCPSSubscriptionsSecure"
     "DCPSParticipantMessageSecure", "DCPSParticipantStatelessMessage", "DCPSParticipantVolatileMessageSecure"
    and a sample DCPS topic*/

  /* Now call the function. */
  result = access_control->get_topic_sec_attributes(
      access_control,
      local_permissions_handle,
      TOPIC_NAMES[SEC_TOPIC_DCPS_SHAPE],
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);

  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_CAN_NOT_FIND_TOPIC_IN_DOMAIN_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, topic_invalid_param, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_TopicSecurityAttributes attributes;
  bool result;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_topic_sec_attributes != NULL);
  assert(access_control->get_topic_sec_attributes != 0);

  result = create_local_identity(0, "Test_Governance_full.p7s");
  CU_ASSERT_FATAL(result);

  memset(&attributes, 0, sizeof(attributes));

  /* Now call the function. */

  result = access_control->get_topic_sec_attributes(
      access_control,
      local_permissions_handle,
      NULL,
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_topic_sec_attributes(
      access_control,
      local_permissions_handle,
      "",
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_topic_sec_attributes(
      access_control,
      local_permissions_handle,
      "Shape",
      NULL,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  result = access_control->get_topic_sec_attributes(
      access_control,
      local_permissions_handle + 12345,
      "Shape",
      &attributes,
      &exception);

  CU_ASSERT_FATAL(!result);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_PARAMETER_CODE);

  //reset control values
  memset(&attributes, 0, sizeof(attributes));
  reset_exception(&exception);

  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, participant_2nd_rule, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_ParticipantSecurityAttributes attributes;
  bool result;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->get_participant_sec_attributes != NULL);
  assert(access_control->get_participant_sec_attributes != 0);

  result = create_local_identity(30, "Test_Governance_full.p7s");
  CU_ASSERT_FATAL(result);

  memset(&attributes, 0, sizeof(attributes));

  result = access_control->get_participant_sec_attributes(
      access_control,
      local_permissions_handle,
      &attributes,
      &exception);
  CU_ASSERT(result);

  /*
     * Expect these values based on these options, which is the 2nd domain rule
     * in the Test_Governance_full.p7s (selected because of domain id 30):
     *
     *    <allow_unauthenticated_participants>1</allow_unauthenticated_participants>
     *    <enable_join_access_control>0</enable_join_access_control>
     *    <discovery_protection_kind>SIGN</discovery_protection_kind>
     *    <liveliness_protection_kind>ENCRYPT</liveliness_protection_kind>
     *    <rtps_protection_kind>NONE</rtps_protection_kind>
     */
  CU_ASSERT(attributes.allow_unauthenticated_participants == true);
  CU_ASSERT(attributes.is_access_protected == false);
  CU_ASSERT(attributes.is_discovery_protected == true);
  CU_ASSERT(attributes.is_liveliness_protected == true);
  CU_ASSERT(attributes.is_rtps_protected == false);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED) ==
            0);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED) ==
            0);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED) ==
            DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED) ==
            0);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED) ==
            0);
  CU_ASSERT((attributes.plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED) ==
            0);

  result = access_control->return_participant_sec_attributes(
      access_control,
      &attributes,
      &exception);
  CU_ASSERT(result);

  clear_local_identity();
  plugins_fini();
}

static void test_liveliness_discovery_participant_attr(
    DDS_Security_PermissionsHandle hdl,
    bool liveliness_protected,
    DDS_Security_unsigned_long liveliness_mask,
    bool discovery_protected,
    DDS_Security_unsigned_long discovery_mask)
{
  DDS_Security_unsigned_long mask = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID |
                                    liveliness_mask |
                                    discovery_mask;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_ParticipantSecurityAttributes attr;
  bool result;

  CU_ASSERT_FATAL(access_control->get_participant_sec_attributes != NULL);
  assert(access_control->get_participant_sec_attributes != 0);

  memset(&attr, 0, sizeof(attr));

  result = access_control->get_participant_sec_attributes(
      access_control,
      hdl,
      &attr,
      &exception);
  CU_ASSERT(result);

  CU_ASSERT(attr.allow_unauthenticated_participants == false);
  CU_ASSERT(attr.is_access_protected == true);
  CU_ASSERT(attr.is_discovery_protected == discovery_protected);
  CU_ASSERT(attr.is_liveliness_protected == liveliness_protected);
  CU_ASSERT(attr.is_rtps_protected == false);
  CU_ASSERT(attr.plugin_participant_attributes == mask);

  result = access_control->return_participant_sec_attributes(
      access_control,
      &attr,
      &exception);
  CU_ASSERT(result);
}

static void test_liveliness_discovery_writer_attr(
    const char *topic_name,
    DDS_Security_PermissionsHandle hdl,
    bool liveliness_protected,
    bool discovery_protected,
    bool submsg_protected,
    DDS_Security_unsigned_long submsg_mask)
{
  DDS_Security_unsigned_long mask = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID | submsg_mask;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_EndpointSecurityAttributes attr;
  DDS_Security_DataTagQosPolicy data_tag;
  DDS_Security_PartitionQosPolicy *partition = NULL;
  bool result;

  CU_ASSERT_FATAL(access_control->get_datawriter_sec_attributes != NULL);
  assert(access_control->get_datawriter_sec_attributes != 0);

  memset(&attr, 0, sizeof(attr));

  result = access_control->get_datawriter_sec_attributes(
      access_control,
      hdl,
      topic_name,
      partition,
      &data_tag,
      &attr,
      &exception);
  CU_ASSERT_FATAL(result);

  CU_ASSERT(attr.is_read_protected == false);
  CU_ASSERT(attr.is_write_protected == false);
  CU_ASSERT(attr.is_submessage_protected == submsg_protected);
  CU_ASSERT(attr.is_payload_protected == false);
  CU_ASSERT(attr.is_key_protected == false);
  CU_ASSERT(attr.is_discovery_protected == discovery_protected);
  CU_ASSERT(attr.is_liveliness_protected == liveliness_protected);
  CU_ASSERT(attr.plugin_endpoint_attributes == mask);

  result = access_control->return_datawriter_sec_attributes(
      access_control,
      &attr,
      &exception);
  CU_ASSERT(result);
}

static void test_liveliness_discovery_reader_attr(
    const char *topic_name,
    DDS_Security_PermissionsHandle hdl,
    bool liveliness_protected,
    bool discovery_protected,
    bool submsg_protected,
    DDS_Security_unsigned_long submsg_mask)
{
  DDS_Security_unsigned_long mask = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID | submsg_mask;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_EndpointSecurityAttributes attr;
  DDS_Security_DataTagQosPolicy data_tag;
  DDS_Security_PartitionQosPolicy *partition = NULL;
  bool result;

  CU_ASSERT_FATAL(access_control->get_datareader_sec_attributes != NULL);
  assert(access_control->get_datareader_sec_attributes != 0);

  memset(&attr, 0, sizeof(attr));

  result = access_control->get_datareader_sec_attributes(
      access_control,
      hdl,
      topic_name,
      partition,
      &data_tag,
      &attr,
      &exception);
  CU_ASSERT_FATAL(result);

  CU_ASSERT(attr.is_read_protected == false);
  CU_ASSERT(attr.is_write_protected == false);
  CU_ASSERT(attr.is_submessage_protected == submsg_protected);
  CU_ASSERT(attr.is_payload_protected == false);
  CU_ASSERT(attr.is_key_protected == false);
  CU_ASSERT(attr.is_discovery_protected == discovery_protected);
  CU_ASSERT(attr.is_liveliness_protected == liveliness_protected);
  CU_ASSERT(attr.plugin_endpoint_attributes == mask);

  result = access_control->return_datareader_sec_attributes(
      access_control,
      &attr,
      &exception);
  CU_ASSERT(result);
}

static void test_liveliness_discovery_attr(
    const char *governance,
    bool liveliness_protected,
    DDS_Security_unsigned_long liveliness_mask,
    bool discovery_protected,
    DDS_Security_unsigned_long discovery_mask)
{
  DDS_Security_unsigned_long submsg_liveliness_mask = 0;
  DDS_Security_unsigned_long submsg_discovery_mask = 0;
  bool result;

  result = plugins_init();
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(access_control != NULL);

  result = create_local_identity(0, governance);
  CU_ASSERT_FATAL(result);

  /* For some endpoints, the submsg encryption mask depends on either the
     * discovery or liveliness mask. */
  if (liveliness_mask & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED)
  {
    submsg_liveliness_mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
  }
  if (liveliness_mask & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED)
  {
    submsg_liveliness_mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
  }
  if (discovery_mask & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED)
  {
    submsg_discovery_mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
  }
  if (discovery_mask & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED)
  {
    submsg_discovery_mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
  }

  /* Participant attributes */

  test_liveliness_discovery_participant_attr(
      local_permissions_handle,
      liveliness_protected,
      liveliness_mask,
      discovery_protected,
      discovery_mask);

  /* Writer attributes */

  /* User topic. */
  test_liveliness_discovery_writer_attr(
      "Kinematics",
      local_permissions_handle,
      liveliness_protected,
      discovery_protected,
      false /* submsg_protected     */,
      0 /* submsg_mask          */);

  /* Builtin topic. */
  test_liveliness_discovery_writer_attr(
      "DCPSPublication",
      local_permissions_handle,
      false /* liveliness_protected */,
      false /* discovery_protected  */,
      false /* submsg_protected     */,
      0 /* submsg_mask          */);

  /* Security (normal) builtin topic. */
  test_liveliness_discovery_writer_attr(
      "DCPSPublicationsSecure",
      local_permissions_handle,
      false /* liveliness_protected */,
      false /* discovery_protected  */,
      discovery_protected /* submsg_protected     */,
      submsg_discovery_mask /* submsg_mask          */);

  /* Security (liveliness affected) builtin topic. */
  test_liveliness_discovery_writer_attr(
      "DCPSParticipantMessageSecure",
      local_permissions_handle,
      false /* liveliness_protected */,
      false /* discovery_protected  */,
      liveliness_protected /* submsg_protected     */,
      submsg_liveliness_mask /* submsg_mask          */);

  /* Reader attributes */

  /* User topic. */
  test_liveliness_discovery_reader_attr(
      "Kinematics",
      local_permissions_handle,
      liveliness_protected,
      discovery_protected,
      false /* submsg_protected     */,
      false /* submsg_mask          */);

  /* Builtin topic. */
  test_liveliness_discovery_reader_attr(
      "DCPSPublication",
      local_permissions_handle,
      false /* liveliness_protected */,
      false /* discovery_protected  */,
      false /* submsg_protected     */,
      0 /* submsg_mask          */);

  /* Security (normal) builtin topic. */
  test_liveliness_discovery_reader_attr(
      "DCPSPublicationsSecure",
      local_permissions_handle,
      false /* liveliness_protected */,
      false /* discovery_protected  */,
      discovery_protected /* submsg_protected     */,
      submsg_discovery_mask /* submsg_mask          */);

  /* Security (liveliness affected) builtin topic. */
  test_liveliness_discovery_reader_attr(
      "DCPSParticipantMessageSecure",
      local_permissions_handle,
      false /* liveliness_protected */,
      false /* discovery_protected  */,
      liveliness_protected /* submsg_protected     */,
      submsg_liveliness_mask /* submsg_mask          */);

  clear_local_identity();
  plugins_fini();
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, liveliness_discovery_clear, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  /*
     * Expect these values based on these options, which is the 1st domain rule
     * in the Test_Governance_liveliness_discovery_clear.p7s (selected because of domain id 0):
     *
     *   <allow_unauthenticated_participants>false</allow_unauthenticated_participants>
     *   <enable_join_access_control>true</enable_join_access_control>
     *   <discovery_protection_kind>NONE</discovery_protection_kind>
     *   <liveliness_protection_kind>NONE</liveliness_protection_kind>
     *   <rtps_protection_kind>NONE</rtps_protection_kind>
     *   <topic_access_rules>
     *       <topic_rule>
     *           <topic_expression>*</topic_expression>
     *           <enable_liveliness_protection>false</enable_liveliness_protection>
     *           <enable_discovery_protection>false</enable_discovery_protection>
     *           <enable_read_access_control>false</enable_read_access_control>
     *           <enable_write_access_control>false</enable_write_access_control>
     *           <metadata_protection_kind>NONE</metadata_protection_kind>
     *           <data_protection_kind>NONE</data_protection_kind>
     *       </topic_rule>
     *   </topic_access_rules>
     */
  test_liveliness_discovery_attr(
      "Test_Governance_liveliness_discovery_clear.p7s",
      /* liveliness_protected */
      false,
      /* liveliness_mask      */
      0,
      /* discovery_protected  */
      false,
      /* discovery_mask       */
      0);
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, liveliness_discovery_encrypted, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  /*
     * Expect these values based on these options, which is the 1st domain rule
     * in the Test_Governance_liveliness_discovery_clear.p7s (selected because of domain id 0):
     *
     *   <allow_unauthenticated_participants>false</allow_unauthenticated_participants>
     *   <enable_join_access_control>true</enable_join_access_control>
     *   <discovery_protection_kind>ENCRYPT</discovery_protection_kind>
     *   <liveliness_protection_kind>ENCRYPT</liveliness_protection_kind>
     *   <rtps_protection_kind>NONE</rtps_protection_kind>
     *   <topic_access_rules>
     *       <topic_rule>
     *           <topic_expression>*</topic_expression>
     *           <enable_liveliness_protection>true</enable_liveliness_protection>
     *           <enable_discovery_protection>true</enable_discovery_protection>
     *           <enable_read_access_control>false</enable_read_access_control>
     *           <enable_write_access_control>false</enable_write_access_control>
     *           <metadata_protection_kind>NONE</metadata_protection_kind>
     *           <data_protection_kind>NONE</data_protection_kind>
     *       </topic_rule>
     *   </topic_access_rules>
     */
  test_liveliness_discovery_attr(
      "Test_Governance_liveliness_discovery_encrypted.p7s",
      /* liveliness_protected */
      true,
      /* liveliness_mask      */
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED,
      /* discovery_protected  */
      true,
      /* discovery_mask       */
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED);
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, liveliness_discovery_signed, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  /*
     * Expect these values based on these options, which is the 1st domain rule
     * in the Test_Governance_liveliness_discovery_clear.p7s (selected because of domain id 0):
     *
     *   <allow_unauthenticated_participants>false</allow_unauthenticated_participants>
     *   <enable_join_access_control>true</enable_join_access_control>
     *   <discovery_protection_kind>SIGN</discovery_protection_kind>
     *   <liveliness_protection_kind>SIGN</liveliness_protection_kind>
     *   <rtps_protection_kind>NONE</rtps_protection_kind>
     *   <topic_access_rules>
     *       <topic_rule>
     *           <topic_expression>*</topic_expression>
     *           <enable_liveliness_protection>true</enable_liveliness_protection>
     *           <enable_discovery_protection>true</enable_discovery_protection>
     *           <enable_read_access_control>false</enable_read_access_control>
     *           <enable_write_access_control>false</enable_write_access_control>
     *           <metadata_protection_kind>NONE</metadata_protection_kind>
     *           <data_protection_kind>NONE</data_protection_kind>
     *       </topic_rule>
     *   </topic_access_rules>
     */
  test_liveliness_discovery_attr(
      "Test_Governance_liveliness_discovery_signed.p7s",
      /* liveliness_protected */
      true,
      /* liveliness_mask      */
      0,
      /* discovery_protected  */
      true,
      /* discovery_mask       */
      0);
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, liveliness_discovery_encrypted_and_authenticated, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  /*
     * Expect these values based on these options, which is the 1st domain rule
     * in the Test_Governance_liveliness_discovery_clear.p7s (selected because of domain id 0):
     *
     *   <allow_unauthenticated_participants>false</allow_unauthenticated_participants>
     *   <enable_join_access_control>true</enable_join_access_control>
     *   <discovery_protection_kind>ENCRYPT_WITH_ORIGIN_AUTHENTICATION</discovery_protection_kind>
     *   <liveliness_protection_kind>ENCRYPT_WITH_ORIGIN_AUTHENTICATION</liveliness_protection_kind>
     *   <rtps_protection_kind>NONE</rtps_protection_kind>
     *   <topic_access_rules>
     *       <topic_rule>
     *           <topic_expression>*</topic_expression>
     *           <enable_liveliness_protection>true</enable_liveliness_protection>
     *           <enable_discovery_protection>true</enable_discovery_protection>
     *           <enable_read_access_control>false</enable_read_access_control>
     *           <enable_write_access_control>false</enable_write_access_control>
     *           <metadata_protection_kind>NONE</metadata_protection_kind>
     *           <data_protection_kind>NONE</data_protection_kind>
     *       </topic_rule>
     *   </topic_access_rules>
     */
  test_liveliness_discovery_attr(
      "Test_Governance_liveliness_discovery_encrypted_and_authenticated.p7s",
      /* liveliness_protected */
      true,
      /* liveliness_mask      */
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED |
          DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED,
      /* discovery_protected  */
      true,
      /* discovery_mask       */
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED |
          DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED);
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, liveliness_discovery_signed_and_authenticated, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  /*
     * Expect these values based on these options, which is the 1st domain rule
     * in the Test_Governance_liveliness_discovery_clear.p7s (selected because of domain id 0):
     *
     *   <allow_unauthenticated_participants>false</allow_unauthenticated_participants>
     *   <enable_join_access_control>true</enable_join_access_control>
     *   <discovery_protection_kind>SIGN_WITH_ORIGIN_AUTHENTICATION</discovery_protection_kind>
     *   <liveliness_protection_kind>SIGN_WITH_ORIGIN_AUTHENTICATION</liveliness_protection_kind>
     *   <rtps_protection_kind>NONE</rtps_protection_kind>
     *   <topic_access_rules>
     *       <topic_rule>
     *           <topic_expression>*</topic_expression>
     *           <enable_liveliness_protection>true</enable_liveliness_protection>
     *           <enable_discovery_protection>true</enable_discovery_protection>
     *           <enable_read_access_control>false</enable_read_access_control>
     *           <enable_write_access_control>false</enable_write_access_control>
     *           <metadata_protection_kind>NONE</metadata_protection_kind>
     *           <data_protection_kind>NONE</data_protection_kind>
     *       </topic_rule>
     *   </topic_access_rules>
     */
  test_liveliness_discovery_attr(
      "Test_Governance_liveliness_discovery_signed_and_authenticated.p7s",
      /* liveliness_protected */
      true,
      /* liveliness_mask      */
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED,
      /* discovery_protected  */
      true,
      /* discovery_mask       */
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED);
}

CU_Test(ddssec_builtin_get_xxx_sec_attributes, liveliness_discovery_different, .init = suite_get_xxx_sec_attributes_init, .fini = suite_get_xxx_sec_attributes_fini)
{
  /*
     * Expect these values based on these options, which is the 1st domain rule
     * in the Test_Governance_liveliness_discovery_clear.p7s (selected because of domain id 0):
     *
     *   <allow_unauthenticated_participants>false</allow_unauthenticated_participants>
     *   <enable_join_access_control>true</enable_join_access_control>
     *   <discovery_protection_kind>ENCRYPT</discovery_protection_kind>
     *   <liveliness_protection_kind>NONE</liveliness_protection_kind>
     *   <rtps_protection_kind>NONE</rtps_protection_kind>
     *   <topic_access_rules>
     *       <topic_rule>
     *           <topic_expression>*</topic_expression>
     *           <enable_liveliness_protection>false</enable_liveliness_protection>
     *           <enable_discovery_protection>true</enable_discovery_protection>
     *           <enable_read_access_control>false</enable_read_access_control>
     *           <enable_write_access_control>false</enable_write_access_control>
     *           <metadata_protection_kind>NONE</metadata_protection_kind>
     *           <data_protection_kind>NONE</data_protection_kind>
     *       </topic_rule>
     *   </topic_access_rules>
     */
  test_liveliness_discovery_attr(
      "Test_Governance_liveliness_discovery_different.p7s",
      /* liveliness_protected */
      false,
      /* liveliness_mask      */
      0,
      /* discovery_protected  */
      true,
      /* discovery_mask       */
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED);
}
