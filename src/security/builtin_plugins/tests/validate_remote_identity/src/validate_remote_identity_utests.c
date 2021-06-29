
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

/* Test helper includes. */
#include "common/src/loader.h"

#include "dds/ddsi/ddsi_security_msg.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "auth_tokens.h"

static const char * SUBJECT_NAME_IDENTITY_CERT      = "CN=CHAM-574 client,O=Some Company,ST=Some-State,C=NL";
static const char * SUBJECT_NAME_IDENTITY_CA        = "CN=CHAM-574 authority,O=Some Company,ST=Some-State,C=NL";

static const char * SUBJECT_NAME_IDENTITY_CERT_2    = "CN=CHAM-574_1 client,O=Some Company,ST=Some-State,C=NL";
static const char * SUBJECT_NAME_IDENTITY_CA_2      = "CN=CHAM-574_1 authority,O=Some Company,ST=Some-State,C=NL";


static const char * RSA_2048_ALGORITHM_NAME         = "RSA-2048";
//static const char * EC_PRIME256V1_ALGORITHM_NAME    = "EC-prime256v1";

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
static DDS_Security_GUID_t local_participant_guid;

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
    uint32_t i;

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
    ex->code = 0;
    ex->minor_code = 0;
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
initialize_identity_token_w_sn(
    DDS_Security_IdentityToken *token,
    const char *certSubjName,
    const char *certAlgo,
    const char *caSubjName,
    const char *caAlgo)
{
    memset(token, 0, sizeof(*token));

    token->class_id = ddsrt_strdup(DDS_AUTHTOKEN_CLASS_ID);
    token->properties._maximum = 4;
    token->properties._length  = 4;
    token->properties._buffer = DDS_Security_PropertySeq_allocbuf(4);

    token->properties._buffer[0].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CERT_SN);
    token->properties._buffer[0].value = ddsrt_strdup(certSubjName);
    token->properties._buffer[0].propagate = true;

    token->properties._buffer[1].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CERT_ALGO);
    token->properties._buffer[1].value = ddsrt_strdup(certAlgo);
    token->properties._buffer[1].propagate = true;

    token->properties._buffer[2].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CA_SN);
    token->properties._buffer[2].value = ddsrt_strdup(caSubjName);
    token->properties._buffer[2].propagate = true;

    token->properties._buffer[3].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CA_ALGO);
    token->properties._buffer[3].value = ddsrt_strdup(caAlgo);
    token->properties._buffer[3].propagate = true;
}

static void
deinitialize_identity_token(
    DDS_Security_IdentityToken *token)
{
    DDS_Security_DataHolder_deinit(token);
}


static int
create_local_identity(void)
{
    int res = 0;
    DDS_Security_ValidationResult_t result;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
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
clear_local_identity(void)
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
check_auth_request_token(
    DDS_Security_AuthRequestMessageToken *token,
    int notNil)
{
    if (notNil) {
        if (!token->class_id ||
            (strcmp(token->class_id, DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID) != 0)) {
            printf("AuthRequestMessageToken has invalid class_id\n");
            return 0;
        }

        if (token->binary_properties._length != 1 ||
            token->binary_properties._buffer == NULL) {
            printf("AuthRequestMessageToken has binary_properties\n");
            return 0;
        }

        if (!token->binary_properties._buffer[0].name ||
            (strcmp(token->binary_properties._buffer[0].name, DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE) != 0)) {
            printf("AuthRequestMessageToken has invalid property name\n");
            return 0;
        }

        if (token->binary_properties._buffer[0].value._length != 32 ||
            token->binary_properties._buffer[0].value._buffer == NULL) {
            printf("AuthRequestMessageToken has invalid property value\n");
            return 0;
        }
    } else {
        if ((strlen(token->class_id) != 0)             ||
            (token->properties._length != 0)           ||
            (token->properties._maximum != 0)          ||
            (token->binary_properties._buffer != NULL) ||
            (token->binary_properties._length != 0)    ||
            (token->binary_properties._maximum != 0)   ||
            (token->binary_properties._buffer != NULL) ) {
            printf("AuthRequestMessageToken is not a TokenNil\n");
            return 0;
        }
    }
    return 1;
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


static void
set_remote_participant_guid(
    DDS_Security_GUID_t *guid,
    int higher)
{
    int i;

    memcpy(guid, &local_participant_guid, sizeof(*guid));

    for (i = 0; i < 12; i++) {
        int index = (i + 4) % 12;
        if (higher) {
            if (guid->prefix[index] < 0xFF) {
                guid->prefix[index]++;
                /*NOTE: It was giving warning ("unsigned char from ‘int’ may alter its value")  with below
                guid->prefix[index] += 1;
                 */
                break;
            }
        } else {
            if (guid->prefix[index] > 0) {
                guid->prefix[index]--;
                /*NOTE: It was giving warning ("unsigned char from ‘int’ may alter its value")  with below
                guid->prefix[index] -= 1;
                 */
                break;
            }
        }
    }
}


CU_Init(ddssec_builtin_validate_remote_identity)
{
    int res = 0;

    /* Only need the authentication plugin. */
    plugins = load_plugins(NULL   /* Access Control */,
                           &auth  /* Authentication */,
                           NULL   /* Cryptograpy    */,
                           NULL);
    if (plugins) {
        res = create_local_identity();
    } else {
        res = -1;
    }
    return res;
}

CU_Clean(ddssec_builtin_validate_remote_identity)
{
    clear_local_identity();
    unload_plugins(plugins);
    return 0;
}

CU_Test(ddssec_builtin_validate_remote_identity,happy_day_nil_auth_req )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_AuthRequestMessageToken local_auth_request_token = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_IdentityToken remote_identity_token;
    DDS_Security_GUID_t remote_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    /* Check if we actually have validate_local_identity function. */
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_remote_identity != NULL);
    assert (auth->validate_remote_identity != NULL);

    if (local_identity_handle == DDS_SECURITY_HANDLE_NIL) {
        return;
    }

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    set_remote_participant_guid(&remote_participant_guid, 1);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle,
                &local_auth_request_token,
                NULL,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid,
                &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST);
    if (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST) {
        CU_ASSERT (remote_identity_handle != DDS_SECURITY_HANDLE_NIL);
        CU_ASSERT (check_auth_request_token(&local_auth_request_token, 1));
    }

    reset_exception(&exception);
    deinitialize_identity_token(&remote_identity_token);
    DDS_Security_DataHolder_deinit(&local_auth_request_token);

    if ((result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST) ||
        (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)) {
         DDS_Security_boolean success = auth->return_identity_handle(auth, remote_identity_handle, &exception);
         CU_ASSERT_TRUE (success);

         if (!success) {
             printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
         }
         reset_exception(&exception);
     }
}

CU_Test(ddssec_builtin_validate_remote_identity,happy_day_with_auth_req )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_AuthRequestMessageToken local_auth_request_token = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_AuthRequestMessageToken remote_auth_request_token;
    DDS_Security_IdentityToken remote_identity_token;
    DDS_Security_GUID_t remote_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    /* Check if we actually have validate_local_identity function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_remote_identity != NULL);
    assert (auth->validate_remote_identity != 0);

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_auth_request_token(&remote_auth_request_token);
    set_remote_participant_guid(&remote_participant_guid, 0);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle,
                &local_auth_request_token,
                &remote_auth_request_token,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid,
                &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT (remote_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT (check_auth_request_token(&local_auth_request_token, 0));

    reset_exception(&exception);
    deinitialize_identity_token(&remote_identity_token);
    DDS_Security_DataHolder_deinit(&remote_auth_request_token);
    DDS_Security_DataHolder_deinit(&local_auth_request_token);

    success = auth->return_identity_handle(auth, remote_identity_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_remote_identity,invalid_parameters )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_AuthRequestMessageToken local_auth_request_token;
    DDS_Security_AuthRequestMessageToken remote_auth_request_token;
    DDS_Security_IdentityToken remote_identity_token;
    DDS_Security_GUID_t remote_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    /* Check if we actually have validate_local_identity function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_remote_identity != NULL);
    assert (auth->validate_remote_identity != 0);

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_auth_request_token(&remote_auth_request_token);
    set_remote_participant_guid(&remote_participant_guid, 1);

    result = auth->validate_remote_identity(
                NULL, &remote_identity_handle, &local_auth_request_token, &remote_auth_request_token,
                local_identity_handle, &remote_identity_token, &remote_participant_guid, &exception);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->validate_remote_identity(
                auth, NULL, &local_auth_request_token, &remote_auth_request_token,
                local_identity_handle, &remote_identity_token, &remote_participant_guid, &exception);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->validate_remote_identity(
                auth, &remote_identity_handle, NULL, &remote_auth_request_token,
                local_identity_handle, &remote_identity_token, &remote_participant_guid, &exception);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->validate_remote_identity(
                auth, &remote_identity_handle, &local_auth_request_token, &remote_auth_request_token,
                local_identity_handle, NULL, &remote_participant_guid, &exception);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->validate_remote_identity(
                auth, &remote_identity_handle, &local_auth_request_token, &remote_auth_request_token,
                local_identity_handle, &remote_identity_token, NULL, &exception);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    DDS_Security_DataHolder_deinit(&remote_auth_request_token);
    deinitialize_identity_token(&remote_identity_token);
}

CU_Test(ddssec_builtin_validate_remote_identity,unknown_local_identity )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle unknown_identity_handle = 0x56;
    DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_AuthRequestMessageToken local_auth_request_token;
    DDS_Security_AuthRequestMessageToken remote_auth_request_token;
    DDS_Security_IdentityToken remote_identity_token;
    DDS_Security_GUID_t remote_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    /* Check if we actually have validate_local_identity function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_remote_identity != NULL);
    assert (auth->validate_remote_identity != 0);

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_auth_request_token(&remote_auth_request_token);
    set_remote_participant_guid(&remote_participant_guid, 0);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle,
                &local_auth_request_token,
                &remote_auth_request_token,
                unknown_identity_handle,
                &remote_identity_token,
                &remote_participant_guid,
                &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_remote_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);
    deinitialize_identity_token(&remote_identity_token);
    DDS_Security_DataHolder_deinit(&remote_auth_request_token);
}


CU_Test(ddssec_builtin_validate_remote_identity,invalid_remote_identity_token )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_AuthRequestMessageToken local_auth_request_token;
    DDS_Security_AuthRequestMessageToken remote_auth_request_token;
    DDS_Security_IdentityToken remote_identity_token;
    DDS_Security_GUID_t remote_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    /* Check if we actually have validate_local_identity function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_remote_identity != NULL);
    assert (auth->validate_remote_identity != 0);

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_auth_request_token(&remote_auth_request_token);
    set_remote_participant_guid(&remote_participant_guid, 0);

    ddsrt_free(remote_identity_token.class_id);
    remote_identity_token.class_id = ddsrt_strdup("DDS:Auth:PKI-PH:1.0");

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle,
                &local_auth_request_token,
                &remote_auth_request_token,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid,
                &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    ddsrt_free(remote_identity_token.class_id);
    remote_identity_token.class_id = ddsrt_strdup("DDS:Auth:PKI-DH:2.0");

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle,
                &local_auth_request_token,
                &remote_auth_request_token,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid,
                &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    deinitialize_identity_token(&remote_identity_token);
    DDS_Security_DataHolder_deinit(&remote_auth_request_token);
}


CU_Test(ddssec_builtin_validate_remote_identity,invalid_auth_req_token )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_AuthRequestMessageToken local_auth_request_token;
    DDS_Security_AuthRequestMessageToken remote_auth_request_token;
    DDS_Security_IdentityToken remote_identity_token;
    DDS_Security_GUID_t remote_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    unsigned char *futureChallenge;

    /* Check if we actually have validate_local_identity function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_remote_identity != NULL);
    assert (auth->validate_remote_identity != 0);

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_auth_request_token(&remote_auth_request_token);
    set_remote_participant_guid(&remote_participant_guid, 0);

    /* check invalid class_id (empty) string for class_id */
    ddsrt_free(remote_auth_request_token.class_id);
    remote_auth_request_token.class_id = ddsrt_strdup("");

    result = auth->validate_remote_identity(
            auth,
            &remote_identity_handle,
            &local_auth_request_token,
            &remote_auth_request_token,
            local_identity_handle,
            &remote_identity_token,
            &remote_participant_guid,
            &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    /* check invalid class_id (NULL) string for class_id */
    ddsrt_free(remote_auth_request_token.class_id);
    remote_auth_request_token.class_id = NULL;

    result = auth->validate_remote_identity(
            auth,
            &remote_identity_handle,
            &local_auth_request_token,
            &remote_auth_request_token,
            local_identity_handle,
            &remote_identity_token,
            &remote_participant_guid,
            &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    /* check invalid class_id string for class_id */
    remote_auth_request_token.class_id = ddsrt_strdup("DDS:Auth:PKI-DH:2.0+AuthReq");

    result = auth->validate_remote_identity(
            auth,
            &remote_identity_handle,
            &local_auth_request_token,
            &remote_auth_request_token,
            local_identity_handle,
            &remote_identity_token,
            &remote_participant_guid,
            &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    /* check invalid property name (empty) for future_challenge */
    ddsrt_free(remote_auth_request_token.class_id);
    remote_auth_request_token.class_id = ddsrt_strdup(DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID);
    ddsrt_free(remote_auth_request_token.binary_properties._buffer[0].name);
    remote_auth_request_token.binary_properties._buffer[0].name = ddsrt_strdup("");

    result = auth->validate_remote_identity(
            auth,
            &remote_identity_handle,
            &local_auth_request_token,
            &remote_auth_request_token,
            local_identity_handle,
            &remote_identity_token,
            &remote_participant_guid,
            &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    /* check invalid property name (NULL) for future_challenge */
    ddsrt_free(remote_auth_request_token.binary_properties._buffer[0].name);
    remote_auth_request_token.binary_properties._buffer[0].name = NULL;

    result = auth->validate_remote_identity(
            auth,
            &remote_identity_handle,
            &local_auth_request_token,
            &remote_auth_request_token,
            local_identity_handle,
            &remote_identity_token,
            &remote_participant_guid,
            &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    /* check invalid property name for future_challenge */
    ddsrt_free(remote_auth_request_token.binary_properties._buffer[0].name);
    remote_auth_request_token.binary_properties._buffer[0].name = ddsrt_strdup("challenge");

    result = auth->validate_remote_identity(
            auth,
            &remote_identity_handle,
            &local_auth_request_token,
            &remote_auth_request_token,
            local_identity_handle,
            &remote_identity_token,
            &remote_participant_guid,
            &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    /* check missing future_challenge property*/
    ddsrt_free(remote_auth_request_token.binary_properties._buffer[0].name);
    remote_auth_request_token.binary_properties._buffer[0].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE);
    futureChallenge = remote_auth_request_token.binary_properties._buffer[0].value._buffer;
    remote_auth_request_token.binary_properties._buffer[0].value._buffer = NULL;
    remote_auth_request_token.binary_properties._buffer[0].value._length = 0;
    remote_auth_request_token.binary_properties._buffer[0].value._maximum = 0;

    result = auth->validate_remote_identity(
            auth,
            &remote_identity_handle,
            &local_auth_request_token,
            &remote_auth_request_token,
            local_identity_handle,
            &remote_identity_token,
            &remote_participant_guid,
            &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    /* check incorrect future_challenge property, too small */
    remote_auth_request_token.binary_properties._buffer[0].value._buffer = futureChallenge;
    remote_auth_request_token.binary_properties._buffer[0].value._length = 16;
    remote_auth_request_token.binary_properties._buffer[0].value._maximum = 32;

    result = auth->validate_remote_identity(
            auth,
            &remote_identity_handle,
            &local_auth_request_token,
            &remote_auth_request_token,
            local_identity_handle,
            &remote_identity_token,
            &remote_participant_guid,
            &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    /* check incorrect future_challenge property: value is NULL */
    remote_auth_request_token.binary_properties._buffer[0].value._buffer = NULL;
    remote_auth_request_token.binary_properties._buffer[0].value._length = 32;
    remote_auth_request_token.binary_properties._buffer[0].value._maximum = 32;

    result = auth->validate_remote_identity(
            auth,
            &remote_identity_handle,
            &local_auth_request_token,
            &remote_auth_request_token,
            local_identity_handle,
            &remote_identity_token,
            &remote_participant_guid,
            &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    remote_auth_request_token.binary_properties._buffer[0].value._buffer = futureChallenge;

    deinitialize_identity_token(&remote_identity_token);
    DDS_Security_DataHolder_deinit(&remote_auth_request_token);
}

CU_Test(ddssec_builtin_validate_remote_identity,already_validated_same_token )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_IdentityHandle remote_identity_handle2 = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_AuthRequestMessageToken local_auth_request_token = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_AuthRequestMessageToken remote_auth_request_token;
    DDS_Security_IdentityToken remote_identity_token;
    DDS_Security_GUID_t remote_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    /* Check if we actually have validate_local_identity function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_remote_identity != NULL);
    assert (auth->validate_remote_identity != 0);

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_auth_request_token(&remote_auth_request_token);
    set_remote_participant_guid(&remote_participant_guid, 0);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle,
                &local_auth_request_token,
                &remote_auth_request_token,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid,
                &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT (remote_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT (check_auth_request_token(&local_auth_request_token, 0));

    reset_exception(&exception);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle2,
                &local_auth_request_token,
                &remote_auth_request_token,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid,
                &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT (remote_identity_handle == remote_identity_handle2);
    CU_ASSERT (check_auth_request_token(&local_auth_request_token, 0));

    reset_exception(&exception);

    deinitialize_identity_token(&remote_identity_token);
    DDS_Security_DataHolder_deinit(&remote_auth_request_token);
    DDS_Security_DataHolder_deinit(&local_auth_request_token);

    success = auth->return_identity_handle(auth, remote_identity_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_remote_identity,already_validated_different_token )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_IdentityHandle remote_identity_handle2 = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_AuthRequestMessageToken local_auth_request_token = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_AuthRequestMessageToken remote_auth_request_token;
    DDS_Security_IdentityToken remote_identity_token;
    DDS_Security_IdentityToken remote_identity_token2;
    DDS_Security_GUID_t remote_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    /* Check if we actually have validate_local_identity function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->validate_remote_identity != NULL);
    assert (auth->validate_remote_identity != 0);

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_auth_request_token(&remote_auth_request_token);
    set_remote_participant_guid(&remote_participant_guid, 0);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle,
                &local_auth_request_token,
                &remote_auth_request_token,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid,
                &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT_FATAL (remote_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT (check_auth_request_token(&local_auth_request_token, 0));

    reset_exception(&exception);

    initialize_identity_token_w_sn(
            &remote_identity_token2,
            SUBJECT_NAME_IDENTITY_CERT_2, RSA_2048_ALGORITHM_NAME,
            SUBJECT_NAME_IDENTITY_CA_2, RSA_2048_ALGORITHM_NAME);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle2,
                &local_auth_request_token,
                &remote_auth_request_token,
                local_identity_handle,
                &remote_identity_token2,
                &remote_participant_guid,
                &exception);

    if (result == DDS_SECURITY_VALIDATION_FAILED) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    deinitialize_identity_token(&remote_identity_token);
    deinitialize_identity_token(&remote_identity_token2);
    DDS_Security_DataHolder_deinit(&remote_auth_request_token);
    DDS_Security_DataHolder_deinit(&local_auth_request_token);

    success = auth->return_identity_handle(auth, remote_identity_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
}

