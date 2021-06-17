



/* CUnit includes. */
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/dds_security_api.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include <stdio.h>
#include <string.h>
#include "dds/ddsrt/environ.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "assert.h"
#include "auth_tokens.h"

/* Test helper includes. */
#include "common/src/loader.h"

/* Private header include */

#ifdef _WIN32
/* supposedly WinSock2 must be included before openssl 1.0.2 headers otherwise winsock will be used */
#include <WinSock2.h>
#endif
#include <openssl/opensslv.h>

static const char * SUBJECT_NAME_IDENTITY_CERT      = "CN=CHAM-574 client,O=Some Company,ST=Some-State,C=NL";
static const char * SUBJECT_NAME_IDENTITY_CA        = "CN=CHAM-574 authority,O=Some Company,ST=Some-State,C=NL";

static const char * RSA_2048_ALGORITHM_NAME         = "RSA-2048";

static const char * AUTH_HANDSHAKE_REQUEST_TOKEN_CLASS_ID = "DDS:Auth:PKI-DH:1.0+Req";

static const char * AUTH_DSIGN_ALGO_RSA_NAME   = "RSASSA-PSS-SHA256";
static const char * AUTH_KAGREE_ALGO_ECDH_NAME = "ECDH+prime256v1-CEUM";
/* static const char * AUTH_KAGREE_ALGO_RSA_NAME  = "DH+MODP-2048-256"; */



static const char *identity_certificate =
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


static const char *identity_ca =
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


static const char *private_key =
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



static struct plugins_hdl *plugins = NULL;
static dds_security_authentication *auth = NULL;
static DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle remote_identity_handle1 = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle remote_identity_handle2 = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_AuthRequestMessageToken g_local_auth_request_token = DDS_SECURITY_TOKEN_INIT;
static DDS_Security_AuthRequestMessageToken g_remote_auth_request_token = DDS_SECURITY_TOKEN_INIT;
static const DDS_Security_BinaryProperty_t *challenge1 = NULL;
static const DDS_Security_BinaryProperty_t *challenge2 = NULL;


static void
dds_security_property_init(
    DDS_Security_PropertySeq *seq,
    DDS_Security_unsigned_long size)
{
    seq->_length = size;
    seq->_maximum = size;
    seq->_buffer = ddsrt_malloc(size * sizeof(DDS_Security_Property_t));
    memset(seq->_buffer, 0, size * sizeof(DDS_Security_Property_t));
}

static void
dds_security_property_deinit(
    DDS_Security_PropertySeq *seq)
{
    uint32_t  i;

    for (i = 0; i < seq->_length; i++) {
        ddsrt_free(seq->_buffer[i].name);
        ddsrt_free(seq->_buffer[i].value);
    }
    ddsrt_free(seq->_buffer);
}

static void
reset_exception(
    DDS_Security_SecurityException *ex)
{
    ex->minor_code = 0;
    ex->code = 0;
    ddsrt_free(ex->message);
    ex->message = NULL;
}

static void
initialize_identity_token(
    DDS_Security_IdentityToken *token,
    const char *certAlgo,
    const char *caAlgo)
{
    memset(token, 0, sizeof(*token));

    token->class_id = ddsrt_strdup(DDS_AUTHTOKEN_CLASS_ID);
    token->properties._maximum = 4;
    token->properties._length  = 4;
    token->properties._buffer = DDS_Security_PropertySeq_allocbuf(4);

    token->properties._buffer[0].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CERT_SN);
    token->properties._buffer[0].value = ddsrt_strdup(SUBJECT_NAME_IDENTITY_CERT);
    token->properties._buffer[0].propagate = true;

    token->properties._buffer[1].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CERT_ALGO);
    token->properties._buffer[1].value = ddsrt_strdup(certAlgo);
    token->properties._buffer[1].propagate = true;

    token->properties._buffer[2].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CA_SN);
    token->properties._buffer[2].value = ddsrt_strdup(SUBJECT_NAME_IDENTITY_CA);
    token->properties._buffer[2].propagate = true;

    token->properties._buffer[3].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CA_ALGO);
    token->properties._buffer[3].value = ddsrt_strdup(caAlgo);
    token->properties._buffer[3].propagate = true;
}

static void
fill_auth_request_token(
     DDS_Security_AuthRequestMessageToken *token)
{
    uint32_t i;
    uint32_t len = 32;
    unsigned char *challenge;

    challenge = ddsrt_malloc(len);

    for (i = 0; i < len; i++) {
        challenge[i] = (unsigned char)(0xFF - i);
    }

    memset(token, 0, sizeof(*token));

    token->class_id = ddsrt_strdup(DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID);
    token->binary_properties._maximum = 1;
    token->binary_properties._length = 1;
    token->binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
    token->binary_properties._buffer->name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE);

    token->binary_properties._buffer->value._maximum = len;
    token->binary_properties._buffer->value._length = len;
    token->binary_properties._buffer->value._buffer = challenge;
}

static const DDS_Security_BinaryProperty_t *
find_binary_property(
    DDS_Security_DataHolder *token,
    const char *name)
{
    const DDS_Security_BinaryProperty_t *result = NULL;
    uint32_t i;

    for (i = 0; i < token->binary_properties._length && !result; i++) {
        if (token->binary_properties._buffer[i].name && (strcmp(token->binary_properties._buffer[i].name, name) == 0)) {
            result = &token->binary_properties._buffer[i];
        }
    }

    return result;
}


static void
deinitialize_identity_token(
    DDS_Security_IdentityToken *token)
{
    DDS_Security_DataHolder_deinit(token);
}

static int
validate_local_identity(void)
{
    int res = 0;
    DDS_Security_ValidationResult_t result;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GUID_t local_participant_guid;
    DDS_Security_GuidPrefix_t prefix = {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb};
    DDS_Security_EntityId_t entityId = {{0xb0,0xb1,0xb2},0x1};

    memset(&local_participant_guid, 0, sizeof(local_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    memset(&participant_qos, 0, sizeof(participant_qos));
    dds_security_property_init(&participant_qos.property.value, 3);
    participant_qos.property.value._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
    participant_qos.property.value._buffer[0].value = ddsrt_strdup(identity_certificate);
    participant_qos.property.value._buffer[1].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
    participant_qos.property.value._buffer[1].value = ddsrt_strdup(identity_ca);
    participant_qos.property.value._buffer[2].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
    participant_qos.property.value._buffer[2].value = ddsrt_strdup(private_key);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &local_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        res = -1;
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    return res;
}

static void
release_local_identity(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    if (local_identity_handle != DDS_SECURITY_HANDLE_NIL) {
        success = auth->return_identity_handle(auth, local_identity_handle, &exception);
        if (!success) {
            printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
        }
        reset_exception(&exception);
    }
}

static int
validate_remote_identities (void)
{
    int res = 0;
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityToken remote_identity_token;
    static DDS_Security_AuthRequestMessageToken local_auth_request_token = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_GUID_t remote_participant_guid1;
    DDS_Security_GUID_t remote_participant_guid2;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix1 = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab};
    DDS_Security_GuidPrefix_t prefix2 = {0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb};
    DDS_Security_EntityId_t entityId = {{0xb0,0xb1,0xb2},0x1};

    memcpy(&remote_participant_guid1.prefix, &prefix1, sizeof(prefix1));
    memcpy(&remote_participant_guid1.entityId, &entityId, sizeof(entityId));
    memcpy(&remote_participant_guid2.prefix, &prefix2, sizeof(prefix2));
    memcpy(&remote_participant_guid2.entityId, &entityId, sizeof(entityId));

    if (local_identity_handle == DDS_SECURITY_HANDLE_NIL) {
        return -1;
    }

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle1,
                &g_local_auth_request_token,
                NULL,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid1,
                &exception);

    if ((result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST) &&
        (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)) {
        printf("validate_remote_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    reset_exception(&exception);

    fill_auth_request_token(&g_remote_auth_request_token);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle2,
                &local_auth_request_token,
                &g_remote_auth_request_token,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid2,
                &exception);

    if ((result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST) &&
            (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)) {
        printf("validate_remote_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    reset_exception(&exception);

    deinitialize_identity_token(&remote_identity_token);
    DDS_Security_DataHolder_deinit(&local_auth_request_token);

    challenge1 = find_binary_property(&g_local_auth_request_token, DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE);
    challenge2 = find_binary_property(&g_remote_auth_request_token, DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE);

    return res;
}

static void
release_remote_identities(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    if (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL) {
        success = auth->return_identity_handle(auth, remote_identity_handle1, &exception);
        if (!success) {
            printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
        }
        reset_exception(&exception);
    }
    if (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL) {
        success = auth->return_identity_handle(auth, remote_identity_handle2, &exception);
        if (!success) {
            printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
        }
        reset_exception(&exception);
    }

    DDS_Security_DataHolder_deinit(&g_local_auth_request_token);
    DDS_Security_DataHolder_deinit(&g_remote_auth_request_token);
}

static void
fill_local_participant_data(
    DDS_Security_OctetSeq *pdata,
    uint32_t length)
{
    uint32_t i;

    pdata->_length = pdata->_maximum = length;
    pdata->_buffer = ddsrt_malloc(length);

    for (i = 0; i < length; i++) {
        pdata->_buffer[i] = (unsigned char)(i % 256);
    }
}

static void
release_local_participant_data(
    DDS_Security_OctetSeq *pdata)
{
    if (pdata) {
        ddsrt_free(pdata->_buffer);
    }
}

CU_Init(ddssec_builtin_validate_begin_handshake_request)
{
    int res = 0;

    /* Only need the authentication plugin. */
    plugins = load_plugins(NULL   /* Access Control */,
                           &auth  /* Authentication */,
                           NULL   /* Cryptograpy    */,
                           &(const struct ddsi_domaingv){ .handshake_include_optional = true });
    if (plugins) {
        res = validate_local_identity();
        if (res >= 0) {
            res = validate_remote_identities();
        }
    } else {
        res = -1;
    }

    return res;
}

CU_Clean(ddssec_builtin_validate_begin_handshake_request)
{
    release_local_identity();
    release_remote_identities();
    unload_plugins(plugins);
    return 0;
}

static bool
compare_octet_seq(
    const DDS_Security_OctetSeq *seq1,
    const DDS_Security_OctetSeq *seq2)
{
    int r;
    if (seq1 && seq2) {
        r = (int)(seq2->_length - seq1->_length);
        if (r == 0) {
            r = memcmp(seq1->_buffer, seq2->_buffer, seq1->_length);
        }
    } else if (seq1 == seq2) {
        r = 0;
    } else {
        r = (seq2 > seq1) ? 1 : -1;
    }
    return r;
}

static bool
valid_c_id_property(
    const char *certificate,
    const DDS_Security_OctetSeq *value)
{
    if (value->_length == 0) {
        CU_FAIL("c.id has no value");
        return false;
    }
    if (strncmp(certificate, (const char *)value->_buffer, value->_length) != 0) {
        return false;
    }
    return true;
}

static bool
valid_string_value(
    const char *expected,
    const DDS_Security_OctetSeq *value)
{
    size_t len = strlen(expected) + 1;

    if (strncmp(expected, (const char *)value->_buffer, len) != 0) {
        return false;
    }

    return true;
}



static bool
validate_handshake_token(
    DDS_Security_HandshakeMessageToken *token,
    const DDS_Security_OctetSeq *challenge)
{
    const DDS_Security_BinaryProperty_t *property;

    if (!token->class_id || strcmp(token->class_id, AUTH_HANDSHAKE_REQUEST_TOKEN_CLASS_ID) != 0) {
        CU_FAIL("HandshakeMessageToken incorrect class_id");
    } else if ((property = find_binary_property(token, DDS_AUTHTOKEN_PROP_C_ID)) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_C_ID"' not found");
    } else if (!valid_c_id_property(&identity_certificate[6], &property->value)) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_C_ID"' value is invalid");
    } else if (find_binary_property(token, DDS_AUTHTOKEN_PROP_C_PDATA) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_C_PDATA"' not found");
    } else if ((property = find_binary_property(token, DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO)) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO"' not found");
    } else if (!valid_string_value(AUTH_DSIGN_ALGO_RSA_NAME, &property->value)) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO"' incorrect value");
    } else if ((property = find_binary_property(token, DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO)) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO"' not found");
    } else if (!valid_string_value(AUTH_KAGREE_ALGO_ECDH_NAME, &property->value)) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO"' incorrect value");
    } else if (find_binary_property(token, DDS_AUTHTOKEN_PROP_HASH_C1) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_HASH_C1"' not found");
    } else if (find_binary_property(token, DDS_AUTHTOKEN_PROP_DH1) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_DH1"' not found");
    } else if ((property = find_binary_property(token, DDS_AUTHTOKEN_PROP_CHALLENGE1)) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_CHALLENGE1"' not found");
    } else if (challenge && compare_octet_seq(challenge, &property->value) != 0) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_CHALLENGE1"' incorrect value");
    } else {
        return true;
    }

    return false;
}



CU_Test(ddssec_builtin_validate_begin_handshake_request,happy_day_challenge)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_OctetSeq local_participant_data;
    DDS_Security_boolean success;

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert (auth->begin_handshake_request != 0);

    fill_local_participant_data(&local_participant_data, 82);

    result = auth->begin_handshake_request(
                    auth,
                    &handshake_handle,
                    &handshake_token,
                    local_identity_handle,
                    remote_identity_handle2,
                    &local_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE) {
        printf("begin_handshake_request failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT(handshake_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT(validate_handshake_token(&handshake_token, NULL));

    reset_exception(&exception);

    release_local_participant_data(&local_participant_data);

    success= auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    DDS_Security_DataHolder_deinit(&handshake_token);
}

CU_Test(ddssec_builtin_validate_begin_handshake_request,happy_day_future_challenge)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_OctetSeq local_participant_data;
    DDS_Security_boolean success;

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert (auth->begin_handshake_request != 0);

    fill_local_participant_data(&local_participant_data, 82);

    result = auth->begin_handshake_request(
                    auth,
                    &handshake_handle,
                    &handshake_token,
                    local_identity_handle,
                    remote_identity_handle1,
                    &local_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE) {
        printf("begin_handshake_request failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT(handshake_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT(validate_handshake_token(&handshake_token, &challenge1->value));

    reset_exception(&exception);

    release_local_participant_data(&local_participant_data);

    success = auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    DDS_Security_DataHolder_deinit(&handshake_token);
}


CU_Test(ddssec_builtin_validate_begin_handshake_request,invalid_arguments)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_OctetSeq local_participant_data;

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert (auth->begin_handshake_request != 0);

    fill_local_participant_data(&local_participant_data, 82);

    result = auth->begin_handshake_request(
                    auth,
                    NULL,
                    &handshake_token,
                    local_identity_handle,
                    remote_identity_handle1,
                    &local_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->begin_handshake_request(
                    auth,
                    &handshake_handle,
                    NULL,
                    local_identity_handle,
                    remote_identity_handle1,
                    &local_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->begin_handshake_request(
                    auth,
                    &handshake_handle,
                    &handshake_token,
                    0x1234598,
                    remote_identity_handle1,
                    &local_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->begin_handshake_request(
                     auth,
                     &handshake_handle,
                     &handshake_token,
                     local_identity_handle,
                     0x1234598,
                     &local_participant_data,
                     &exception);

     if (result != DDS_SECURITY_VALIDATION_OK) {
         printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
     }
     CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);
     reset_exception(&exception);

     result = auth->begin_handshake_request(
                     auth,
                     &handshake_handle,
                     &handshake_token,
                     local_identity_handle,
                     remote_identity_handle1,
                     NULL,
                     &exception);

     if (result != DDS_SECURITY_VALIDATION_OK) {
         printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
     }
     CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);
     reset_exception(&exception);

     release_local_participant_data(&local_participant_data);
}

CU_Test(ddssec_builtin_validate_begin_handshake_request,return_handle)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_OctetSeq local_participant_data;
    DDS_Security_boolean success;

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert (auth->begin_handshake_request != 0);

    fill_local_participant_data(&local_participant_data, 82);

    result = auth->begin_handshake_request(
                    auth,
                    &handshake_handle,
                    &handshake_token,
                    local_identity_handle,
                    remote_identity_handle2,
                    &local_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE) {
        printf("begin_handshake_request failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT (handshake_handle != DDS_SECURITY_HANDLE_NIL);

    reset_exception(&exception);

    release_local_participant_data(&local_participant_data);

    success = auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    success = auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_FALSE (success);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    DDS_Security_DataHolder_deinit(&handshake_token);
}
