/** @file qos_utests.c
 *  @brief Unit tests for qos APIs
 *
 */
#include <assert.h>

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_thread.h"
#include "ddsi__xevent.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/dds_security_api_authentication.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/openssl_support.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "common/src/loader.h"
#include "config_env.h"
#include "auth_tokens.h"
#include "ac_tokens.h"

static const char * SUBJECT_NAME_PERMISSIONS_CA     = "/C=NL/ST=OV/L=Locality Name/OU=Organizational Unit Name/O=Example Signer CA/CN=Example CA/emailAddress=authority@identitycaltd.org";
static const char * RSA_2048_ALGORITHM_NAME         = "RSA-2048";

static const char * PERMISSIONS_CA_CERT_FILE             = "Permissions_CA_Cert.pem";
static const char * ALICE_PERMISSIONS_FILE             = "Example_Permissions_Alice.p7s";
static const char * BOB_PERMISSIONS_FILE             = "Example_Permissions_Bob.p7s";
static const char * ALICE_IDENTITY_CERT_FILE        = "Example_Alice_Cert.pem";
static const char * BOB_IDENTITY_CERT_FILE          = "Example_Bob_Cert.pem";
static const char * AUTH_DSIGN_ALGO_RSA_NAME   = "RSASSA-PSS-SHA256";
static const char * AUTH_KAGREE_ALGO_ECDH_NAME = "ECDH+prime256v1-CEUM";
static DDS_Security_PermissionsHandle remote_permissions_handle = DDS_SECURITY_HANDLE_NIL;
static dds_security_authentication_listener auth_listener;
static dds_security_access_control_listener access_control_listener;

#define IDENTITY_CA_FILE "Identity_CA_Cert.pem"
#define IDENTITY_CA_KEY_FILE "Identity_CA_Private_Key.pem"

static const char *identity_ca =/*Identity_CA_Cert.pem*/
        "data:,-----BEGIN CERTIFICATE-----\n"
        "MIIEOTCCAyGgAwIBAgIJAPq0b61+PT2WMA0GCSqGSIb3DQEBCwUAMIGyMQswCQYD\n"
        "VQQGEwJOTDELMAkGA1UECAwCT1YxFjAUBgNVBAcMDUxvY2FsaXR5IE5hbWUxITAf\n"
        "BgNVBAsMGE9yZ2FuaXphdGlvbmFsIFVuaXQgTmFtZTEaMBgGA1UECgwRRXhhbXBs\n"
        "ZSBTaWduZXIgQ0ExEzARBgNVBAMMCkV4YW1wbGUgQ0ExKjAoBgkqhkiG9w0BCQEW\n"
        "G2F1dGhvcml0eUBpZGVudGl0eWNhbHRkLm9yZzAeFw0xOTAyMTIxMDIxNTJaFw0y\n"
        "OTAyMDkxMDIxNTJaMIGyMQswCQYDVQQGEwJOTDELMAkGA1UECAwCT1YxFjAUBgNV\n"
        "BAcMDUxvY2FsaXR5IE5hbWUxITAfBgNVBAsMGE9yZ2FuaXphdGlvbmFsIFVuaXQg\n"
        "TmFtZTEaMBgGA1UECgwRRXhhbXBsZSBTaWduZXIgQ0ExEzARBgNVBAMMCkV4YW1w\n"
        "bGUgQ0ExKjAoBgkqhkiG9w0BCQEWG2F1dGhvcml0eUBpZGVudGl0eWNhbHRkLm9y\n"
        "ZzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKCBJ1r/2AcSHorpA2G2\n"
        "WR0CvGHPhhY2x93twW91LCJVOVzO0LuOscZXSkWDtAhyhy1EZN6r+4aLbMku/wVJ\n"
        "kdjHPD+WSVEZn70LxYSgxiUwXalpa5RQeTkEHll5cSgtE8kSD4/HIxBsbwizeDVV\n"
        "g8SWpBVb044GM4O3TDbCug9F7GJFzqcbSHQZnHO+3nWu6f21BEU7PZjrFox1NREN\n"
        "g3H7WmNISx4DOK9bJcWS/i4qJjTxjQPPFmzGvRgO2FfWP+xYb70x/iOeKsML2y+d\n"
        "XZqL99yzfP1dnpDtBzCTqJJizfuNMD6gvIXyk2PUy3FpAYoI9BvUehdWCP/okikx\n"
        "5jsCAwEAAaNQME4wHQYDVR0OBBYEFL7LTHLvMsEeUDjMYeW4+DcXn62PMB8GA1Ud\n"
        "IwQYMBaAFL7LTHLvMsEeUDjMYeW4+DcXn62PMAwGA1UdEwQFMAMBAf8wDQYJKoZI\n"
        "hvcNAQELBQADggEBAIDJ4g6aVGIVXDSQ5R2yY9I82zsRf3k+yRF/BBkqBP1XXYRA\n"
        "6lk7Wk4y6+DmL9qbVG/xrkTCC066J8kVblOyfFP1LHAzlNOQE7aU+tyrAufW4fpz\n"
        "f/Gv8PBQUTQGr8vNqLUuEdoQjzARm8g7L3qeXhIKjiWsWi99ibnm/jTjol1GleIX\n"
        "RudKSSGyMcB2VgRjCEEIYrkXdkIfrznKcJxzUw3dsGx277dB+4iFLcqf1YDpoRe2\n"
        "aSwbtgx+lLZ4KXWtikBSmLSRBq9j2aGtKO08kru0U3jQo6B4Bvzp1KuJCBiktueY\n"
        "yNRfgh8ggNERYF/SpVr/ivm3RM+mnWd3QpmVolw=\n"
        "-----END CERTIFICATE-----\n";

static const char *alice_private_key =
        "data:,-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIEpAIBAAKCAQEA0Vn9YYllmrC/VVkxWFq6TzsmWn/niMydsE0hhm4Eam00gw3v\n"
        "M4xScvBexI0td6tbStnWKz8vBiSVtWY1GDX8ICCBDufJcNIypu9L3DsjBoVTpcjx\n"
        "XmfSpcxsepNaS7IqFPoRd+J+tzrl5qUR0Q52ObI02csp6CwecL8CvfXfc02U+tpP\n"
        "b7iSsT/AcCeAAc10vdSXtnfcOGVLwy+D527kNr5r2VhE2FYdWbrGSOj8hw5eUfRs\n"
        "KpnWlermWy1wzYfmMqkIII1hhY1sRDmMDrLpFeiYkf9B/7HQF+R2tFFFok1dYVpg\n"
        "ZiZUNkONQnDQueThypzu3XB/PwHExILZnBwEtwIDAQABAoIBAHEVxCoQtuKlgOUQ\n"
        "hfgtIiC0WdZe6unZZYCbWXWtLhNzI/964nAc51iRAQ/5Fstis7CuFONNgRA3aOsQ\n"
        "57NJTgToqe4sRIL9+EB2WKsBAr19/Z46+i69tGq9DwfzWr4y4kpsfk0c+sftN8yr\n"
        "9ADSaAhoe+X9uYhhdJwAgfGsw+Qa1BLM5jEexjcLwy1kqXwWKNHrPWU7NW40bPhV\n"
        "Ne/UguklSp43sKoB1esTgRZe+OlQDCsQoXpyjtPYcKzF3Ion6e5rT8xQ1IkAtLTf\n"
        "QJJTGMJGCguXxuDo2VRaa5pwbdRAamp63wD0IAPAf+ClCz4JLwOnOYsgSCZoY3l3\n"
        "3GxWDAkCgYEA/scs7Ft50QOicnJ/DoCjxmpjba0hiqxbz0qAdCyl1llogUKuyCS7\n"
        "sxQQvVTt6jk3gP0UYa3FJREToIGZuqaEu6qdY4GI61woNN5hXbkJC6G3FXI/VB8v\n"
        "Actkas0kp6J8YWsmTqnsihE5Yii0WCBn+AnIpoU3S2N9WT7U8eA6B7sCgYEA0lsJ\n"
        "xqB6NCkKbr+E+V0CGp+S+7zH5dxZtAhDPSKwy2kCYWtDlSVhAzXabTIzF2SW9tJQ\n"
        "JV3FhRfiCWTBrBm/I7AmNbNtXnrexYyXPVBhN6uqzYCB8fcLy+HPNZYF3KtQtU3c\n"
        "WxAyO6ZxSLgtcP3UB6J8Ac6CYB25cRWNsnrvETUCgYA1HpHfNbNQQNG9yuFyxJ9g\n"
        "3w2b8Fzt7MG3lnDxx91Ls5h2WtDWKdJ4o9ZZozt3ejZ4TkvRkclo0QamkF7c65sB\n"
        "BbGK7Zb+e1hmrXbfc5TPOAhUEF3jzByg4ycsnVjnGpmUNiLmg8ctginUrWfsd9U1\n"
        "gdSz41KEBVo9ITyEsZtnwQKBgQCOgIfd3CcNIORlZC8D8wMS4BllmlzdFepa8OIE\n"
        "D3UvR2MKdezho+HVl+zx3nkIFufCK3WJ6r19TVGeRXiCSyrWVWV9KaEkyR4TPAvU\n"
        "yJgja5MZBj6BmXePVdjWl1w/Qns5Z5aoxg8Ro87IkaSPEBVMWsGhQ7HExT40IoLM\n"
        "b0V3JQKBgQCnZRrzaY9xOy72rtyacWjQB2QxnmkZgwDUq0iQwU/EdP72fPV/TtG8\n"
        "PxQqKVPM57XO7qDbyKBt9L3vvmkiC0bABlcMN7UX9H5cHGhnJn6+6muitJlxmtvU\n"
        "IiwlYmN2hcA52Ft9cRVC8+8W6Knx019vBnJozjhzhy7wxwqggmxsxg==\n"
        "-----END RSA PRIVATE KEY-----\n";

static const char *bob_private_key =
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIEogIBAAKCAQEA5Piv5VnOqg9vAWXGrdxH2Ax1svypJNVmNUlM95qGQloeJQPk\n"
        "jSzAEHIBVYAE0eU5gHL3l9EI2mw/bVLsT/REXipj9STPN9NYOVNH2yeD8+KHWNXX\n"
        "miTYJ5YjHP9GHDdKiZ52dn7BYktqUlghlxuY0pjWrqhnNkcJep7YZq4QTbCQcyUV\n"
        "l+9v+lE6/DuSG5K1KNRCQmmnRVjdWmGQogrW8eZ1xNFwHJuZYgKP/q7ieQSYI+YR\n"
        "Mrt1pScjj6FjdL5OMYShrBO7OBYrwDjMnpmox9omF+1S1Gmi1y03b3m1COKX4lID\n"
        "Qpc7gvpwq02ZozEX+kfIs0Ua8EKbVgi23V/2OQIDAQABAoIBACXLry1Kr8R+m7I9\n"
        "XJhiXjGZjOworLr5xs9Q9DWC+lqFiahOhjGPi3yrdPDqGuGS1vUPBTO4O5/icm3X\n"
        "XE6uYYKxuKJEmzf52PxNdPUGBtABOpo9YkN9hXizXcRxlt8deV5SG/ffYIibLke9\n"
        "aH4K/iT1OarG/ZKGE1h8U/hPDz3jcVGpqhwn7K4RtRJO7brYKw1jDJwH64g5gDZ7\n"
        "Y2zcj5zh1JsnOYPx/1HWxDDIbBN9RbOTyKZIMP9OwglbS5RhIblTgM8zT8ZEHQ1s\n"
        "h5dF1jt5pKtcqQelYbfACsTmVCvtJR9Uz98mYE/xpwvJTXbzeycmEccdfuWWFkFB\n"
        "xzQroKECgYEA/VeD9T3/zJ9uls0DBuj+LBu+7mL/RrPuoJrqBIuXvIvddYgmvEkf\n"
        "IhB7OLgLeMv5KvHRn8XWoEcPGoglfbY4jqIML855gFolyXSNJlbSiOPit8Ea9dKi\n"
        "60fHhaa1U+E2xdDOeUilQVCEgnnqSBls1kGqB+81os5GVgo7MzW1+Y0CgYEA51+2\n"
        "PYKLC5KU8r/bWlwjF2DmVJq6tD3O8SonqKsPv9MwGz0AimLO+iNtvEkh2rLVidtK\n"
        "oP0tvIgsu70xqgX4wRBpZB+rk5IS8y3dCVf6TGVFYK9La5/L5BHLAzbf5jybRzH7\n"
        "lT3O94oE7w7gkUiexSM37Wp8FoQG4EPl7MXXBl0CgYBDLE5H232U4v0urQNNdL/Y\n"
        "MC8rBELNm239VbYRKHY+PxOkU0p6CCViId6aRmp8SBE0KtQ7OfjTnKPLlCfksklC\n"
        "wILctjGPL9fvF6FJdiHyvAHkWSZt4cDjA7BKps5ThFbCkr/8dp+itte7xNmy7lLm\n"
        "aJjN68Zb+be6npHd3TL4DQKBgBxHXHTEIc52SfIpdNvkav2OgFhS2QLykvpy1ooM\n"
        "7k3ZuAV8PTaswPNdpSngHl0mgmbpAIQQrahfVGhVxV4sgKzIHrl4DXZp4hsKvftI\n"
        "X3U643Hfuu4ah8cGTbPE3zS6r5fSChfBiCxFGDlHrjbTk2Qw28MOwr/Vvyll4xI/\n"
        "U/qZAoGARuIs5CqcL3NTLyXQGCa3OIeUoqlpMHyy9niJb/S4z6ew6BvI2/uuI//j\n"
        "zyZyRdexTF7rOIFDMqsCz7EpJDO9jjwTeKGZ1n25kFJS2AXHhFIU9mdYF9rZ9yuE\n"
        "yLB7ISyuiPNMhsOOHMHWWTKiqmQdGBc/HTCKRA1Pu34Bcem55PA=\n"
        "-----END RSA PRIVATE KEY-----\n";

static const char *alice_csr =
        "-----BEGIN CERTIFICATE REQUEST-----\n"
        "MIIC9zCCAd8CAQAwgbExCzAJBgNVBAYTAk5MMQswCQYDVQQIDAJPVjEWMBQGA1UE\n"
        "BwwNTG9jYWxpdHkgTmFtZTEhMB8GA1UECwwYT3JnYW5pemF0aW9uYWwgVW5pdCBO\n"
        "YW1lMR0wGwYDVQQKDBRFeGFtcGxlIE9yZ2FuaXphdGlvbjEWMBQGA1UEAwwNQWxp\n"
        "Y2UgRXhhbXBsZTEjMCEGCSqGSIb3DQEJARYUYWxpY2VAZXhhbXBsZWx0ZC5jb20w\n"
        "ggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDRWf1hiWWasL9VWTFYWrpP\n"
        "OyZaf+eIzJ2wTSGGbgRqbTSDDe8zjFJy8F7EjS13q1tK2dYrPy8GJJW1ZjUYNfwg\n"
        "IIEO58lw0jKm70vcOyMGhVOlyPFeZ9KlzGx6k1pLsioU+hF34n63OuXmpRHRDnY5\n"
        "sjTZyynoLB5wvwK99d9zTZT62k9vuJKxP8BwJ4ABzXS91Je2d9w4ZUvDL4PnbuQ2\n"
        "vmvZWETYVh1ZusZI6PyHDl5R9GwqmdaV6uZbLXDNh+YyqQggjWGFjWxEOYwOsukV\n"
        "6JiR/0H/sdAX5Ha0UUWiTV1hWmBmJlQ2Q41CcNC55OHKnO7dcH8/AcTEgtmcHAS3\n"
        "AgMBAAGgADANBgkqhkiG9w0BAQsFAAOCAQEAOqzDicnVFXBhsfRI/Gcmk0kyKAjg\n"
        "UAfADqDT7EyqX31O4C+Flj+G++p4/Yo0W8yP01nSMOF5yB76Eep+hXuLldfL1aKs\n"
        "krnZgIrCXyUSb5pVYHrbhI76eet6TMD8P9gj0okPLOfzyTHrQz9wCAv9FfiNl2y+\n"
        "cK/meQPAByyxSiyW41owlHaMkSgIL/xULJHjR07QRCiKIAY3/mwUQ5I3k4qKibzQ\n"
        "eV4Cus2SEqqXhNsFwZPWQZQmZOXLcEX7g9OsC4iVyGOunUwZdv8BRVThQk+5rGWk\n"
        "dHjeWYmS7pWc8EjLnaSairQiw9tmZ0SQcNc7DjnPdkBU78MHxMW54sKBrw==\n"
        "-----END CERTIFICATE REQUEST-----\n";


static const char *bob_csr =
        "-----BEGIN CERTIFICATE REQUEST-----\n"
        "MIIC8zCCAdsCAQAwga0xCzAJBgNVBAYTAk5MMQswCQYDVQQIDAJPVjEWMBQGA1UE\n"
        "BwwNTG9jYWxpdHkgTmFtZTEhMB8GA1UECwwYT3JnYW5pemF0aW9uYWwgVW5pdCBO\n"
        "YW1lMR0wGwYDVQQKDBRFeGFtcGxlIE9yZ2FuaXphdGlvbjEUMBIGA1UEAwwLQm9i\n"
        "IEV4YW1wbGUxITAfBgkqhkiG9w0BCQEWEmJvYkBleGFtcGxlbHRkLmNvbTCCASIw\n"
        "DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAOT4r+VZzqoPbwFlxq3cR9gMdbL8\n"
        "qSTVZjVJTPeahkJaHiUD5I0swBByAVWABNHlOYBy95fRCNpsP21S7E/0RF4qY/Uk\n"
        "zzfTWDlTR9sng/Pih1jV15ok2CeWIxz/Rhw3SomednZ+wWJLalJYIZcbmNKY1q6o\n"
        "ZzZHCXqe2GauEE2wkHMlFZfvb/pROvw7khuStSjUQkJpp0VY3VphkKIK1vHmdcTR\n"
        "cBybmWICj/6u4nkEmCPmETK7daUnI4+hY3S+TjGEoawTuzgWK8A4zJ6ZqMfaJhft\n"
        "UtRpotctN295tQjil+JSA0KXO4L6cKtNmaMxF/pHyLNFGvBCm1YItt1f9jkCAwEA\n"
        "AaAAMA0GCSqGSIb3DQEBCwUAA4IBAQBr85eN2h2q1dLfSeaJHmyVqmwdj2igMOcK\n"
        "4RllC2LZ1aorfCvMSb5VzAUbVDFwKJOCfM5vk4hXmt9Uxxmr5qHpnBWi5XGoevuf\n"
        "iQ+hdZqS9HIn1/vzNyX6OlCJwVd4qo42RLJFnxyKYnFtZrhA/kdB33Rf+UyRxfoB\n"
        "/Su7pUUR+iIMZeu/C3LT3cZy5phqaWzOsw83m3Hap/UzTt5WAhynRfaYRdXuwACU\n"
        "QscNSTJsYUfSlynzxY2GHgOw3N34x5pLlVGdOBoG4BjjdZVTeyPc/KgpkUYFY3mD\n"
        "jDnSobyp5xr4wcR4RHJCUxCd3/bAAmqKwXY+GnRvOrdKG35Eii0Z\n"
        "-----END CERTIFICATE REQUEST-----\n";


static char *bob_identity_cert = NULL;

typedef enum {
    HANDSHAKE_REQUEST,
    HANDSHAKE_REPLY,
    HANDSHAKE_FINAL
} HandshakeStep_t;


struct octet_seq {
    unsigned char  *data;
    uint32_t  length;
};

static struct plugins_hdl *plugins = NULL;
static dds_security_authentication *auth = NULL;
static dds_security_access_control *access_control = NULL;
static DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle remote_identity_handle2 = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_PermissionsHandle local_permissions_handle = DDS_SECURITY_HANDLE_NIL;
static char *path_to_etc_dir = NULL;
static char *g_path_build_dir = NULL;
static DDS_Security_IdentityHandle identity_handle_for_callback1=DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle identity_handle_for_callback2=DDS_SECURITY_HANDLE_NIL;
static dds_time_t local_expiry_date;
static dds_time_t remote_expiry_date;

#define HANDSHAKE_SIGNATURE_SIZE 6

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
static  unsigned bswap4u (unsigned x)
{
  return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}
#define toBE4u(x) bswap4u (x)
#define fromBE4u(x) bswap4u (x)
#else
#define toBE4u(x) (x)
#define fromBE4u(x) (x)
#endif

static const char * AUTH_HANDSHAKE_REQUEST_TOKEN_CLASS_ID = "DDS:Auth:PKI-DH:1.0+Req";
static const char * AUTH_HANDSHAKE_REPLY_TOKEN_CLASS_ID   = "DDS:Auth:PKI-DH:1.0+Reply";
static const char * AUTH_HANDSHAKE_FINAL_TOKEN_CLASS_ID   = "DDS:Auth:PKI-DH:1.0+Final";
static DDS_Security_AuthRequestMessageToken g_local_auth_request_token = DDS_SECURITY_TOKEN_INIT;
static DDS_Security_AuthRequestMessageToken g_remote_auth_request_token = DDS_SECURITY_TOKEN_INIT;
static const DDS_Security_BinaryProperty_t *challenge1_predefined_glb = NULL;
static const DDS_Security_BinaryProperty_t *challenge2_predefined_glb = NULL;
static DDS_Security_OctetSeq serialized_participant_data = DDS_SECURITY_SEQUENCE_INIT;
static DDS_Security_ParticipantBuiltinTopicData *remote_participant_data1 = NULL;
static DDS_Security_ParticipantBuiltinTopicData *remote_participant_data2 = NULL;
static DDS_Security_ParticipantBuiltinTopicData *remote_participant_data3 = NULL;
static DDS_Security_GUID_t candidate_participant_guid;
static DDS_Security_GUID_t remote_participant_guid1;
static DDS_Security_GUID_t remote_participant_guid2;

static EVP_PKEY *dh_modp_key = NULL;
static EVP_PKEY *dh_ecdh_key = NULL;
static struct octet_seq dh_modp_pub_key = {NULL, 0};
static struct octet_seq dh_ecdh_pub_key = {NULL, 0};
static struct octet_seq invalid_dh_pub_key = {NULL, 0};


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


static dds_time_t
get_certificate_expiry(
    /*_In_*/ X509 *cert)
{
    dds_time_t expiry = DDS_TIME_INVALID;
    ASN1_TIME *ans1;

    assert(cert);

    ans1 = X509_get_notAfter(cert);
    if (ans1 != NULL) {
        int days;
        int seconds;
        if (ASN1_TIME_diff(&days, &seconds, NULL, ans1) != 0) {
            static const dds_duration_t secs_per_day = 86400;
            dds_duration_t delta = DDS_SECS(((dds_duration_t)seconds + ((dds_duration_t)days * secs_per_day)));
            expiry = dds_time() + delta;
            {
                BIO *b;
                b = BIO_new_fp(stdout, BIO_NOCLOSE);
                BIO_printf(b, "[asn1time] ");
                ASN1_TIME_print(b, ans1);
                BIO_printf(b, "\n");
                BIO_free(b);
            }
        }
    }

    return expiry;
}


/* Generate a CA-signed certificate from CSR (Certificate Signing Request) */
static DDS_Security_boolean create_certificate_from_csr(const char* csr, long valid_secs,
                const char* outfile, dds_time_t *expiry)
{

    ASN1_INTEGER *aserial = NULL;
    EVP_PKEY *ca_privkey, *req_pubkey;
    EVP_MD const *digest = NULL;
    X509 *newcert, *cacert;
    X509_NAME *name;
    X509V3_CTX ctx;
    BIO *fp;
    BIO *reqbio = NULL;
    BIO *outbio = NULL;
    X509_REQ *certreq = NULL;
    char *identity_ca_cert_file;
    char *identity_ca_key_file;
    char* certificate_file;

    outbio = BIO_new(BIO_s_file());

    /* ---------------------------------------------------------- *
     * Load the request data in a BIO, then in a x509_REQ struct. *
     * ---------------------------------------------------------- */
    reqbio = BIO_new_mem_buf(csr, -1);

    if (!(certreq = PEM_read_bio_X509_REQ(reqbio, NULL, NULL, NULL))) {
        BIO_printf(outbio, "Error can't read X509 request data into memory\n");
        return false;
    }

    /* -------------------------------------------------------- *
     * Load the signing CA Certificate file                    *
     * ---------------------------------------------------------*/

    ddsrt_asprintf(&identity_ca_cert_file, "%s%s", path_to_etc_dir, IDENTITY_CA_FILE);

    if (!(fp = BIO_new_file(identity_ca_cert_file, "r"))) {
        BIO_printf(outbio, "Error reading CA cert file\n");
        return false;
    }

    if (!(cacert = PEM_read_bio_X509(fp, NULL, NULL, NULL))) {
        BIO_printf(outbio, "Error loading CA cert into memory\n");
        return false;
    }

    BIO_free(fp);

    /* -------------------------------------------------------- *
     * Import CA private key file for signing                   *
     * ---------------------------------------------------------*/

    ddsrt_asprintf(&identity_ca_key_file, "%s%s", path_to_etc_dir, IDENTITY_CA_KEY_FILE);

    if (!(fp = BIO_new_file(identity_ca_key_file, "r"))) {
        BIO_printf(outbio, "Error reading CA private key file\n");
        return false;
    }

    if (!(ca_privkey = PEM_read_bio_PrivateKey(fp, NULL, NULL, NULL))) {
        BIO_printf(outbio, "Error importing key content from file\n");
        return false;
    }

    BIO_free(fp);

    /* --------------------------------------------------------- *
     * Build Certificate with data from request                  *
     * ----------------------------------------------------------*/
    if (!(newcert = X509_new())) {
        BIO_printf(outbio, "Error creating new X509 object\n");
        return false;
    }

    if (X509_set_version(newcert, 2) != 1) {
        BIO_printf(outbio, "Error setting certificate version\n");
        return false;
    }

    /* --------------------------------------------------------- *
     * set the certificate serial number here                    *
     * If there is a problem, the value defaults to '0'          *
     * ----------------------------------------------------------*/
    aserial = ASN1_INTEGER_new();
    ASN1_INTEGER_set(aserial, 0);
    if (!X509_set_serialNumber(newcert, aserial)) {
        BIO_printf(outbio, "Error setting serial number of the certificate\n");
        return false;
    }

    /* --------------------------------------------------------- *
     * Extract the subject name from the request                 *
     * ----------------------------------------------------------*/
    if (!(name = X509_REQ_get_subject_name(certreq)))
        BIO_printf(outbio, "Error getting subject from cert request\n");

    /* --------------------------------------------------------- *
     * Set the new certificate subject name                      *
     * ----------------------------------------------------------*/
    if (X509_set_subject_name(newcert, name) != 1) {
        BIO_printf(outbio, "Error setting subject name of certificate\n");
        return false;
    }

    /* --------------------------------------------------------- *
     * Extract the subject name from the signing CA cert         *
     * ----------------------------------------------------------*/
    if (!(name = X509_get_subject_name(cacert))) {
        BIO_printf(outbio, "Error getting subject from CA certificate\n");
        return false;
    }

    /* --------------------------------------------------------- *
     * Set the new certificate issuer name                       *
     * ----------------------------------------------------------*/
    if (X509_set_issuer_name(newcert, name) != 1) {
        BIO_printf(outbio, "Error setting issuer name of certificate\n");
        return false;
    }

    /* --------------------------------------------------------- *
     * Extract the public key data from the request              *
     * ----------------------------------------------------------*/
    if (!(req_pubkey = X509_REQ_get_pubkey(certreq))) {
        BIO_printf(outbio, "Error unpacking public key from request\n");
        return false;
    }

    /* --------------------------------------------------------- *
     * Optionally: Use the public key to verify the signature    *
     * ----------------------------------------------------------*/
    if (X509_REQ_verify(certreq, req_pubkey) != 1) {
        BIO_printf(outbio, "Error verifying signature on request\n");
        return false;
    }

    /* --------------------------------------------------------- *
     * Set the new certificate public key                        *
     * ----------------------------------------------------------*/
    if (X509_set_pubkey(newcert, req_pubkey) != 1) {
        BIO_printf(outbio, "Error setting public key of certificate\n");
        return false;
    }

    /* ---------------------------------------------------------- *
     * Set X509V3 start date (now) and expiration date (+365 days)*
     * -----------------------------------------------------------*/
    if (!(X509_gmtime_adj(X509_get_notBefore(newcert), -10))) {
        BIO_printf(outbio, "Error setting start time\n");
        return false;
    }

    if (!(X509_gmtime_adj(X509_get_notAfter(newcert), valid_secs))) {
        BIO_printf(outbio, "Error setting expiration time\n");
        return false;
    }

    /* ----------------------------------------------------------- *
     * Add X509V3 extensions                                       *
     * ------------------------------------------------------------*/
    X509V3_set_ctx(&ctx, cacert, newcert, NULL, NULL, 0);

    /* ----------------------------------------------------------- *
     * Set digest type, sign new certificate with CA's private key *
     * ------------------------------------------------------------*/
    digest = EVP_sha256();

    if (!X509_sign(newcert, ca_privkey, digest)) {
        BIO_printf(outbio, "Error signing the new certificate\n");
        return false;
    }

    /* ------------------------------------------------------------ *
     *  write the certificate to file                               *
     * -------------------------------------------------------------*/
    ddsrt_asprintf(&certificate_file, "%s%s", g_path_build_dir, outfile);

    if (!(fp = BIO_new_file(certificate_file, "w"))) {
        BIO_printf(outbio, "Error opening certificate file for write\n");
        return false;
    }
    if (!PEM_write_bio_X509( fp, newcert)) {
        BIO_printf(outbio, "Error writing the signed certificate\n");
        return false;
    }
    BIO_free(fp);

    *expiry = get_certificate_expiry( newcert );

    EVP_PKEY_free(req_pubkey);
    EVP_PKEY_free(ca_privkey);
    X509_REQ_free(certreq);
    ASN1_INTEGER_free( aserial );
    X509_free(newcert);
    X509_free(cacert);
    BIO_free_all(reqbio);
    BIO_free_all(outbio);
    ddsrt_free(certificate_file);
    ddsrt_free( identity_ca_cert_file );
    ddsrt_free( identity_ca_key_file );
    return true;
}

/* fills the participant properties and return identity cert expiry */
static void
fill_participant_qos(
    DDS_Security_Qos *qos,
    long validity_duration,
    const char *governance_filename,
    dds_time_t *identity_expiry)
{
    char *permission_uri;
    char *governance_uri;
    char *permissions_ca_cert_file;
    char *permissions_ca_uri;
    char *identity_file;
    char *identity_uri;
    char *permissions_file;

    create_certificate_from_csr( alice_csr, validity_duration , ALICE_IDENTITY_CERT_FILE, identity_expiry);

    /*permissions ca cert*/
    ddsrt_asprintf(&permissions_ca_cert_file, "%s%s", path_to_etc_dir, PERMISSIONS_CA_CERT_FILE);

    ddsrt_asprintf(&permissions_ca_uri, "file:%s",  permissions_ca_cert_file);

    /*Alice Identity*/
    ddsrt_asprintf(&identity_file, "%s%s", g_path_build_dir, ALICE_IDENTITY_CERT_FILE);

    ddsrt_asprintf(&identity_uri, "file:%s",  identity_file);

    /*Alice Permissions */
    ddsrt_asprintf(&permissions_file, "%s%s", path_to_etc_dir, ALICE_PERMISSIONS_FILE);

    ddsrt_asprintf(&permission_uri, "file:%s",  permissions_file);

    ddsrt_asprintf(&governance_uri, "file:%s%s", path_to_etc_dir, governance_filename);

    memset(qos, 0, sizeof(*qos));
    dds_security_property_init(&qos->property.value, 6);
    qos->property.value._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
    qos->property.value._buffer[0].value = ddsrt_strdup(identity_uri);
    qos->property.value._buffer[1].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
    qos->property.value._buffer[1].value = ddsrt_strdup(identity_ca);
    qos->property.value._buffer[2].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
    qos->property.value._buffer[2].value = ddsrt_strdup(alice_private_key);
    qos->property.value._buffer[3].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_PERMISSIONS_CA);
    qos->property.value._buffer[3].value = ddsrt_strdup(permissions_ca_uri);
    qos->property.value._buffer[4].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_PERMISSIONS);
    qos->property.value._buffer[4].value = ddsrt_strdup(permission_uri);
    qos->property.value._buffer[5].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_GOVERNANCE);
    qos->property.value._buffer[5].value = ddsrt_strdup(governance_uri);

    ddsrt_free(permission_uri);
    ddsrt_free(governance_uri);
    ddsrt_free( permissions_ca_cert_file );
    ddsrt_free( permissions_file );
    ddsrt_free ( identity_file );
    ddsrt_free( identity_uri );
    ddsrt_free( permissions_ca_uri );


}

static void
fill_permissions_token(
    DDS_Security_PermissionsToken *token)
{
    memset(token, 0, sizeof(DDS_Security_PermissionsToken));

    token->class_id = ddsrt_strdup(DDS_ACTOKEN_PERMISSIONS_CLASS_ID);
    token->properties._length = token->properties._maximum = 2;
    token->properties._buffer = DDS_Security_PropertySeq_allocbuf(2);

    token->properties._buffer[0].name = ddsrt_strdup(DDS_ACTOKEN_PROP_PERM_CA_SN);
    token->properties._buffer[0].value = ddsrt_strdup(SUBJECT_NAME_PERMISSIONS_CA);

    token->properties._buffer[1].name = ddsrt_strdup(DDS_ACTOKEN_PROP_PERM_CA_ALGO);
    token->properties._buffer[1].value = ddsrt_strdup(RSA_2048_ALGORITHM_NAME);
}


static int
fill_peer_credential_token(
    DDS_Security_AuthenticatedPeerCredentialToken *token)
{
    int result = 1;
    char *permission_data;
    char *permissions_ca_cert_file;
    char *permissions_file;


    /*permissions ca cert*/
    ddsrt_asprintf(&permissions_ca_cert_file, "%s%s", path_to_etc_dir, PERMISSIONS_CA_CERT_FILE);

    /*permissions ca key*/
    ddsrt_asprintf(&permissions_file, "%s%s", path_to_etc_dir, BOB_PERMISSIONS_FILE);


    memset(token, 0, sizeof(DDS_Security_AuthenticatedPeerCredentialToken));

    permission_data = load_file_contents(permissions_file);

    if (permission_data) {
        token->class_id = ddsrt_strdup(DDS_AUTHTOKEN_CLASS_ID);
        token->properties._length = token->properties._maximum = 2;
        token->properties._buffer = DDS_Security_PropertySeq_allocbuf(2);

        token->properties._buffer[0].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_C_ID);
        token->properties._buffer[0].value = ddsrt_strdup(&bob_identity_cert[0]);

        token->properties._buffer[1].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_C_PERM);
        token->properties._buffer[1].value = permission_data;
    } else {
        ddsrt_free(permission_data);
        result = 0;
    }

    ddsrt_free( permissions_ca_cert_file );
    ddsrt_free( permissions_file );
    return result;
}


static void
serializer_participant_data(
    DDS_Security_ParticipantBuiltinTopicData *pdata,
    unsigned char **buffer,
    size_t *size)
{
    DDS_Security_Serializer serializer;
    serializer = DDS_Security_Serializer_new(1024, 1024);

    DDS_Security_Serialize_ParticipantBuiltinTopicData(serializer, pdata);
    DDS_Security_Serializer_buffer(serializer, buffer, size);
    DDS_Security_Serializer_free(serializer);
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
    token->properties._buffer[0].value = ddsrt_strdup("/C=NL/ST=OV/L=Locality Name/OU=Organizational Unit Name/O=Example Organization/CN=Bob Example/emailAddress=bob@exampleltd.com");
    token->properties._buffer[0].propagate = true;

    token->properties._buffer[1].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CERT_ALGO);
    token->properties._buffer[1].value = ddsrt_strdup(certAlgo);
    token->properties._buffer[1].propagate = true;

    token->properties._buffer[2].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CA_SN);
    token->properties._buffer[2].value = ddsrt_strdup("/C=NL/ST=OV/L=Locality Name/OU=Organizational Unit Name/O=Example Signer CA/CN=Example CA/emailAddress=authority@identitycaltd.org");
    token->properties._buffer[2].propagate = true;

    token->properties._buffer[3].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CA_ALGO);
    token->properties._buffer[3].value = ddsrt_strdup(caAlgo);
    token->properties._buffer[3].propagate = true;
}

static DDS_Security_long
validate_local_identity_and_permissions( uint32_t identity_expiry_duration, dds_time_t *identity_expiry )
{
    DDS_Security_long res = DDS_SECURITY_ERR_OK_CODE;
    DDS_Security_ValidationResult_t result;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb};
    DDS_Security_EntityId_t entityId = {{0xb0,0xb1,0xb2},0x1};
    DDS_Security_GUID_t local_participant_guid;
    DDS_Security_ParticipantBuiltinTopicData *local_participant_data;
    unsigned char *sdata;
    size_t sz;

    memset(&local_participant_guid, 0, sizeof(local_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, (long)identity_expiry_duration, "Example_Governance.p7s", identity_expiry );

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
        res = DDS_SECURITY_ERR_UNDEFINED_CODE;
        printf("validate_local_identity_failed: (%d) %s\n", (int)exception.code, exception.message ? exception.message : "Error message missing");
    }

    reset_exception(&exception);

    if (res == 0) {
        local_permissions_handle = access_control->validate_local_permissions(
                access_control,
                auth,
                local_identity_handle,
                0,
                &participant_qos,
                &exception);

        if (local_permissions_handle == DDS_SECURITY_HANDLE_NIL) {
            printf("validate_local_permissions_failed: (%d) %s\n", (int)exception.code, exception.message ? exception.message : "Error message missing");
            if (exception.code == DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE) {
                /* This can happen on very slow platforms or when doing a valgrind run. */
                res = DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE;
            } else {
                res = DDS_SECURITY_ERR_UNDEFINED_CODE;
            }
        }
    }

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    local_participant_data = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&local_participant_data->key[0], &local_participant_guid, 12);
    /* convert from big-endian format to native format */
    local_participant_data->key[0] = fromBE4u(local_participant_data->key[0]);
    local_participant_data->key[1] = fromBE4u(local_participant_data->key[1]);
    local_participant_data->key[2] = fromBE4u(local_participant_data->key[2]);

    initialize_identity_token(&local_participant_data->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_permissions_token(&local_participant_data->permissions_token );

    local_participant_data->security_info.participant_security_attributes = 0x01;
    local_participant_data->security_info.plugin_participant_security_attributes = 0x02;

    serializer_participant_data(local_participant_data, &sdata, &sz);

    serialized_participant_data._length = serialized_participant_data._maximum = (DDS_Security_unsigned_long) sz;
    serialized_participant_data._buffer = sdata;

    DDS_Security_ParticipantBuiltinTopicData_free(local_participant_data);



    return res;
}

static void
clear_local_identity_and_permissions(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    if (local_permissions_handle != DDS_SECURITY_HANDLE_NIL) {
        success = access_control->return_permissions_handle(access_control, local_permissions_handle, &exception);
        if (!success) {
            printf("return_permission_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
        }
        reset_exception(&exception);
    }

    if (local_identity_handle != DDS_SECURITY_HANDLE_NIL) {
        success = auth->return_identity_handle(auth, local_identity_handle, &exception);
        if (!success) {
            printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
        }
        reset_exception(&exception);
    }

    DDS_Security_OctetSeq_deinit(&serialized_participant_data);
}

static int
set_path_to_etc_dir(void)
{
    int res = 0;
    size_t len;

    len = 1024;
    path_to_etc_dir = ddsrt_malloc(len);
    snprintf(path_to_etc_dir, 1024, "%s/listeners_authentication/etc/", CONFIG_ENV_TESTS_DIR);

    return res;
}

static void set_path_build_dir(void)
{
  ddsrt_asprintf(&g_path_build_dir, "%s/", CONFIG_ENV_BUILD_DIR);
}

/* handshake helper functions */


static DDS_Security_BinaryProperty_t *
find_binary_property(
    DDS_Security_DataHolder *token,
    const char *name)
{
    DDS_Security_BinaryProperty_t *result = NULL;
    uint32_t i;

    for (i = 0; i < token->binary_properties._length && !result; i++) {
        if (token->binary_properties._buffer[i].name && (strcmp(token->binary_properties._buffer[i].name, name) == 0)) {
            result = &token->binary_properties._buffer[i];
        }
    }

    return result;
}


static void
octet_seq_init(
    struct octet_seq *seq,
    unsigned char *data,
    uint32_t size)
{
    seq->data = ddsrt_malloc(size);
    memcpy(seq->data, data, size);
    seq->length = size;
}

static void
octet_seq_deinit(
    struct octet_seq *seq)
{
    ddsrt_free(seq->data);
}
static char *
get_openssl_error_message_for_test(
        void)
{
    BIO *bio = BIO_new(BIO_s_mem());
    char *msg;
    char *buf = NULL;
    size_t len;

    if (bio) {
        ERR_print_errors(bio);
        len = (uint32_t)BIO_get_mem_data (bio, &buf);
        msg = ddsrt_malloc(len + 1);
        memset(msg, 0, len+1);
        memcpy(msg, buf, len);
        BIO_free(bio);
    } else {
        msg = ddsrt_strdup("BIO_new failed");
    }

    return msg;
}


static const BIGNUM *
dh_get_public_key(
    /*_In_ */DH *dhkey)
{
#ifdef AUTH_INCLUDE_DH_ACCESSORS
    const BIGNUM *pubkey, *privkey;
    DH_get0_key(dhkey, &pubkey, &privkey);
    return pubkey;
#else
    return dhkey->pub_key;
#endif
}



/* DH Helper Functions */

static int
create_dh_key_modp_2048(
    EVP_PKEY **pkey)
{
    int r = 0;
    EVP_PKEY *params = NULL;
    EVP_PKEY_CTX *kctx = NULL;
    DH *dh = NULL;

    *pkey = NULL;

    if ((params = EVP_PKEY_new()) == NULL) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate EVP_PKEY: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if ((dh = DH_get_2048_256()) == NULL) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate DH parameter: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (EVP_PKEY_set1_DH(params, dh) <= 0) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to set DH parameter to MODP_2048_256: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if ((kctx = EVP_PKEY_CTX_new(params, NULL)) == NULL) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate KEY context %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (EVP_PKEY_keygen_init(kctx) <= 0) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to initialize KEY context: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (EVP_PKEY_keygen(kctx, pkey) <= 0) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to generate :MODP_2048_256 keys %s", msg);
        ddsrt_free(msg);
        r = -1;
    }

    if (params) EVP_PKEY_free(params);
    if (kctx) EVP_PKEY_CTX_free(kctx);
    if (dh) DH_free(dh);

    return r;
}

static int
get_dh_public_key_modp_2048(
    EVP_PKEY *pkey,
    struct octet_seq *pubkey)
{
    int r = 0;
    DH *dhkey;
    unsigned char *buffer = NULL;
    uint32_t size;
    ASN1_INTEGER *asn1int;

    dhkey = EVP_PKEY_get1_DH(pkey);
    if (!dhkey) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to get DH key from PKEY: %s", msg);
        ddsrt_free(msg);
        r = -1;
        goto fail_get_dhkey;
    }

    asn1int = BN_to_ASN1_INTEGER( dh_get_public_key(dhkey) , NULL);
    if (!asn1int) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to convert DH key to ASN1 integer: %s", msg);
        ddsrt_free(msg);
        r = -1;
        goto fail_get_pubkey;
    }

    size = (uint32_t)i2d_ASN1_INTEGER(asn1int, &buffer);
    octet_seq_init(pubkey, buffer, size);

    ASN1_INTEGER_free(asn1int);
    OPENSSL_free(buffer);

fail_get_pubkey:
    DH_free(dhkey);
fail_get_dhkey:
    return r;
}

static int
create_dh_key_ecdh(
    EVP_PKEY **pkey)
{
    int r = 0;
    EVP_PKEY *params = NULL;
    EVP_PKEY_CTX *pctx = NULL;
    EVP_PKEY_CTX *kctx = NULL;

    *pkey = NULL;

    if ((pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL)) == NULL) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate DH parameter context: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (EVP_PKEY_paramgen_init(pctx) <= 0) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to initialize DH generation context: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1) <= 0) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to set DH generation parameter generation method: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (EVP_PKEY_paramgen(pctx, &params) <= 0) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to generate DH parameters: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if ((kctx = EVP_PKEY_CTX_new(params, NULL)) == NULL) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate KEY context %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (EVP_PKEY_keygen_init(kctx) <= 0) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to initialize KEY context: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (EVP_PKEY_keygen(kctx, pkey) <= 0) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to generate :MODP_2048_256 keys %s", msg);
        ddsrt_free(msg);
        r = -1;
    }

    if (kctx) EVP_PKEY_CTX_free(kctx);
    if (params) EVP_PKEY_free(params);
    if (pctx) EVP_PKEY_CTX_free(pctx);

    return r;
}

static int
get_dh_public_key_ecdh(
    EVP_PKEY *pkey,
    struct octet_seq *pubkey)
{
    int r = 0;
    EC_KEY *eckey = NULL;
    const EC_GROUP *group = NULL;
    const EC_POINT *point = NULL;
    size_t sz;

    if (!(eckey = EVP_PKEY_get1_EC_KEY(pkey))) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to get EC key from PKEY: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (!(point = EC_KEY_get0_public_key(eckey))) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to get public key from ECKEY: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if (!(group = EC_KEY_get0_group(eckey))) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to get group from ECKEY: %s", msg);
        ddsrt_free(msg);
        r = -1;
    } else if ((sz = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, NULL, 0, NULL)) != 0) {
        pubkey->data = ddsrt_malloc(sz);
        pubkey->length = (uint32_t)EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, pubkey->data, sz, NULL);
        if (pubkey->length == 0) {
            char *msg = get_openssl_error_message_for_test();
            printf("Failed to serialize public EC key: %s", msg);
            ddsrt_free(msg);
            octet_seq_deinit(pubkey);
            r = -1;
        }
    } else {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to serialize public EC key: %s", msg);
        ddsrt_free(msg);
        r = -1;
    }

    if (eckey) EC_KEY_free(eckey);

    return r;
}

CU_Init(ddssec_builtin_listeners_auth)
{
    int res = 0;
    dds_openssl_init ();
    ddsi_thread_states_init();

    plugins = load_plugins(&access_control   /* Access Control */,
                           &auth  /* Authentication */,
                           NULL   /* Cryptograpy    */,
                           &(const struct ddsi_domaingv){ .handshake_include_optional = true });

    if (plugins) {
        set_path_build_dir();
        res = set_path_to_etc_dir();
        if (res >= 0) {
           res = create_dh_key_modp_2048(&dh_modp_key);
        }
        if (res >= 0) {
            res = get_dh_public_key_modp_2048(dh_modp_key, &dh_modp_pub_key);
        }
        if (res >= 0) {
           res = create_dh_key_ecdh(&dh_ecdh_key);
        }
        if (res >= 0) {
           res = get_dh_public_key_ecdh(dh_ecdh_key, &dh_ecdh_pub_key);
        }
        if (res >= 0) {
           octet_seq_init(&invalid_dh_pub_key, dh_modp_pub_key.data, dh_modp_pub_key.length);
           invalid_dh_pub_key.data[0] = 0x08;
        }
    } else {
        res = -1;
    }

    dds_openssl_init ();
    return res;
}

CU_Clean(ddssec_builtin_listeners_auth)
{
    octet_seq_deinit(&dh_modp_pub_key);
    octet_seq_deinit(&dh_ecdh_pub_key);
    octet_seq_deinit(&invalid_dh_pub_key);
    if (dh_modp_key) {
        EVP_PKEY_free(dh_modp_key);
    }
    if (dh_ecdh_key) {
        EVP_PKEY_free(dh_ecdh_key);
    }
    unload_plugins(plugins);

    ddsrt_free(path_to_etc_dir);
    return 0;
}



static void
set_binary_property_value(
     DDS_Security_BinaryProperty_t *bp,
     const char *name,
     const unsigned char *data,
     uint32_t length)
{
    assert(bp);
    assert(name);
    assert(data);

    bp->name = ddsrt_strdup(name);
    bp->value._maximum = bp->value._length = length;
    if (length) {
        bp->value._buffer = ddsrt_malloc(length);
        memcpy(bp->value._buffer, data, length);
    } else {
        bp->value._buffer = NULL;
    }
}

static void
set_binary_property_string(
     DDS_Security_BinaryProperty_t *bp,
     const char *name,
     const char *data)
{
    uint32_t length;

    assert(bp);
    assert(name);
    assert(data);

    length = (uint32_t)strlen(data) + 1;
    set_binary_property_value(bp, name, (const unsigned char *)data, length);
}


static DDS_Security_ValidationResult_t
create_asymmetrical_signature_for_test(
     EVP_PKEY *pkey,
     void *data,
     size_t dataLen,
     unsigned char **signature,
     size_t *signatureLen,
     DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    EVP_MD_CTX *mdctx = NULL;
    EVP_PKEY_CTX *kctx = NULL;

    if (!(mdctx = EVP_MD_CTX_create())) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to create signing context: %s", msg);
        ddsrt_free(msg);
        goto err_create_ctx;
    }

    if (EVP_DigestSignInit(mdctx, &kctx, EVP_sha256(), NULL, pkey) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to initialize signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(kctx, RSA_PKCS1_PSS_PADDING) < 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to initialize signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    if (EVP_DigestSignUpdate(mdctx, data, dataLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to update signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    if (EVP_DigestSignFinal(mdctx, NULL, signatureLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to finalize signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    *signature = ddsrt_malloc(*signatureLen);
    if (EVP_DigestSignFinal(mdctx, *signature, signatureLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to finalize signing context: %s", msg);
        ddsrt_free(msg);
        ddsrt_free(*signature);
    }

err_sign:
    EVP_MD_CTX_destroy(mdctx);
err_create_ctx:
    return result;
}


static X509 *
load_certificate(
   const char *data)
{
    X509 *cert = NULL;
    BIO *bio;

    bio = BIO_new_mem_buf((void *) data, -1);
    if (!bio) {
        return NULL;
    }

    cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);

    BIO_free(bio);

    return cert;
}

static int
get_adjusted_participant_guid(
    X509 *cert,
    const DDS_Security_GUID_t *candidate,
    DDS_Security_GUID_t *adjusted)
{
    int result = 0;
    unsigned char high[SHA256_DIGEST_LENGTH], low[SHA256_DIGEST_LENGTH];
    unsigned char *subject;
    DDS_Security_octet hb = 0x80;
    X509_NAME *name;
    unsigned char *tmp = NULL;
    int i;
    int sz;

    name = X509_get_subject_name(cert);
    sz = i2d_X509_NAME(name, &tmp);
    if (sz > 0) {
        subject = ddsrt_malloc((size_t)sz);
        memcpy(subject, tmp, (size_t)sz);
        OPENSSL_free(tmp);

        SHA256(subject, (size_t)sz, high);
        SHA256(&candidate->prefix[0], sizeof(DDS_Security_GuidPrefix_t), low);

        adjusted->entityId = candidate->entityId;
        for (i = 0; i < 6; i++) {
            adjusted->prefix[i] = hb | high[i]>>1;
            hb = (DDS_Security_octet)(high[i]<<7);
        }
        for (i = 0; i < 6; i++) {
            adjusted->prefix[i+6] = low[i];
        }
        ddsrt_free(subject);
        result = 1;
    }

    return result;
}

static DDS_Security_ValidationResult_t
create_signature_for_test(
    EVP_PKEY *pkey,
    const DDS_Security_BinaryProperty_t **binary_properties,
    const uint32_t binary_properties_length,
    unsigned char **signature,
    size_t *signatureLen,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_Serializer serializer;
    unsigned char *buffer;
    size_t size;

    serializer = DDS_Security_Serializer_new(4096, 4096);

    DDS_Security_Serialize_BinaryPropertyArray(serializer,binary_properties, binary_properties_length);
    DDS_Security_Serializer_buffer(serializer, &buffer, &size);

    result = create_asymmetrical_signature_for_test(pkey, buffer, size, signature, signatureLen, ex);

    ddsrt_free(buffer);
    DDS_Security_Serializer_free(serializer);

    return result;
}


static void
deinitialize_identity_token(
    DDS_Security_IdentityToken *token)
{
    DDS_Security_DataHolder_deinit(token);
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

static int
validate_remote_identity (const char *remote_id_certificate)
{
    int res = 0;
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityToken remote_identity_token;
    static DDS_Security_AuthRequestMessageToken local_auth_request_token = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_GUID_t guid1;
    DDS_Security_GUID_t guid2;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix1 = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab};
    DDS_Security_GuidPrefix_t prefix2 = {0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb};
    DDS_Security_EntityId_t entityId = {{0xb0,0xb1,0xb2},0x1};
    X509 *cert;

    memcpy(&guid1.prefix, &prefix1, sizeof(prefix1));
    memcpy(&guid1.entityId, &entityId, sizeof(entityId));
    memcpy(&guid2.prefix, &prefix2, sizeof(prefix2));
    memcpy(&guid2.entityId, &entityId, sizeof(entityId));

    if (local_identity_handle == DDS_SECURITY_HANDLE_NIL) {
        return -1;
    }

    cert = load_certificate(remote_id_certificate);
    if (!cert) {
        return -1;
    }

    if (!get_adjusted_participant_guid(cert, &guid1, &remote_participant_guid1)) {
        X509_free(cert);
        return -1;
    }

    if (!get_adjusted_participant_guid(cert, &guid2, &remote_participant_guid2)) {
        X509_free(cert);
        return -1;
    }

    X509_free(cert);

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);

    result = auth->validate_remote_identity(
                auth,
                &remote_identity_handle,
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

    remote_participant_data1 = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&remote_participant_data1->key[0], &remote_participant_guid1, 12);
    remote_participant_data1->key[0] = fromBE4u(remote_participant_data1->key[0]);
    remote_participant_data1->key[1] = fromBE4u(remote_participant_data1->key[1]);
    remote_participant_data1->key[2] = fromBE4u(remote_participant_data1->key[2]);

    initialize_identity_token(&remote_participant_data1->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_permissions_token(&remote_participant_data1->permissions_token );

    remote_participant_data1->security_info.participant_security_attributes = 0x01;
    remote_participant_data1->security_info.plugin_participant_security_attributes = 0x02;

    remote_participant_data2 = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&remote_participant_data2->key[0], &remote_participant_guid2, 12);
    remote_participant_data2->key[0] = fromBE4u(remote_participant_data2->key[0]);
    remote_participant_data2->key[1] = fromBE4u(remote_participant_data2->key[1]);
    remote_participant_data2->key[2] = fromBE4u(remote_participant_data2->key[2]);

    initialize_identity_token(&remote_participant_data2->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_permissions_token(&remote_participant_data2->permissions_token );

    remote_participant_data2->security_info.participant_security_attributes = 0x01;
    remote_participant_data2->security_info.plugin_participant_security_attributes = 0x02;

    remote_participant_data3 = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&remote_participant_data3->key[0], &candidate_participant_guid, 12);

    initialize_identity_token(&remote_participant_data3->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    fill_permissions_token(&remote_participant_data3->permissions_token );

    remote_participant_data2->security_info.participant_security_attributes = 0x01;
    remote_participant_data2->security_info.plugin_participant_security_attributes = 0x02;

    challenge1_predefined_glb = find_binary_property(&g_remote_auth_request_token, DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE);
    challenge2_predefined_glb = challenge1_predefined_glb;

    return res;
}

static void
release_remote_identities(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    if (remote_identity_handle != DDS_SECURITY_HANDLE_NIL) {
        success = auth->return_identity_handle(auth, remote_identity_handle, &exception);
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

    DDS_Security_ParticipantBuiltinTopicData_free(remote_participant_data1);
    DDS_Security_ParticipantBuiltinTopicData_free(remote_participant_data2);
    DDS_Security_ParticipantBuiltinTopicData_free(remote_participant_data3);
}


static void
fill_handshake_message_token(
    DDS_Security_HandshakeMessageToken *token,
    DDS_Security_ParticipantBuiltinTopicData *pdata,
    const char *certificate,
    const char *dsign,
    const char *kagree,
    const struct octet_seq *diffie_hellman1,
    const unsigned char *challengeData,
    unsigned int challengeDataSize,
    const struct octet_seq *diffie_hellman2,
    const unsigned char *challengeData2,
    unsigned int challengeDataSize2,
    const DDS_Security_BinaryProperty_t *hash1_from_request,
    const DDS_Security_BinaryProperty_t *hash2_from_reply,
    HandshakeStep_t step)
{
    DDS_Security_BinaryProperty_t *tokens;
    DDS_Security_BinaryProperty_t *c_id;
    DDS_Security_BinaryProperty_t *c_perm;
    DDS_Security_BinaryProperty_t *c_pdata;
    DDS_Security_BinaryProperty_t *c_dsign_algo;
    DDS_Security_BinaryProperty_t *c_kagree_algo;
    DDS_Security_BinaryProperty_t *hash_c1;
    DDS_Security_BinaryProperty_t *hash_c2;
    DDS_Security_BinaryProperty_t *dh1;
    DDS_Security_BinaryProperty_t *dh2;
    DDS_Security_BinaryProperty_t *challenge1;
    DDS_Security_BinaryProperty_t *challenge2;
    DDS_Security_BinaryProperty_t *signature;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    unsigned idx;
    unsigned char *serialized_local_participant_data;
    size_t serialized_local_participant_data_size;
    /*unsigned hash[32];*/

    switch( step )
    {

    case HANDSHAKE_REQUEST:
       tokens = DDS_Security_BinaryPropertySeq_allocbuf(8);
       c_id = &tokens[0];
       c_perm = &tokens[1];
       c_pdata = &tokens[2];
       c_dsign_algo = &tokens[3];
       c_kagree_algo = &tokens[4];
       hash_c1 = &tokens[5];
       dh1 = &tokens[6];
       challenge1 = &tokens[7];

        serializer_participant_data(pdata, &serialized_local_participant_data, &serialized_local_participant_data_size);

       /* Store the Identity Certificate associated with the local identify in c.id property */
       if (certificate) {
           set_binary_property_string(c_id, DDS_AUTHTOKEN_PROP_C_ID, certificate);
       } else {
           set_binary_property_string(c_id, DDS_AUTHTOKEN_PROP_C_ID "x", "rubbish");
       }

       /* Store the permission document in the c.perm property */
       set_binary_property_string(c_perm, DDS_AUTHTOKEN_PROP_C_PERM, "permissions_document");

       /* Store the provided local_participant_data in the c.pdata property */
       set_binary_property_value(c_pdata, DDS_AUTHTOKEN_PROP_C_PDATA, serialized_local_participant_data, (uint32_t)serialized_local_participant_data_size);
       ddsrt_free(serialized_local_participant_data);

       /* Set the used signing algorithm descriptor in c.dsign_algo */
       if (dsign) {
           set_binary_property_string(c_dsign_algo, DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO, dsign);
       } else {
           set_binary_property_string(c_dsign_algo, DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO "x", "rubbish");
       }

       /* Set the used key algorithm descriptor in c.kagree_algo */
       if (kagree) {
           set_binary_property_string(c_kagree_algo, DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO, kagree);
       } else {
           set_binary_property_string(c_kagree_algo, DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO "x", "rubbish");
       }

       /* Calculate the hash_c1 */
       {
           DDS_Security_BinaryPropertySeq bseq;
           DDS_Security_Serializer serializer;
           unsigned char hash1_sent_in_request_arr[32];
           unsigned char *buffer;
           size_t size;

           bseq._length = bseq._maximum = 5;
           bseq._buffer = tokens;

           serializer = DDS_Security_Serializer_new(1024, 1024);

           DDS_Security_Serialize_BinaryPropertySeq(serializer, &bseq);
           DDS_Security_Serializer_buffer(serializer, &buffer, &size);
           SHA256(buffer, size, hash1_sent_in_request_arr);
           ddsrt_free(buffer);
           DDS_Security_Serializer_free(serializer);

           set_binary_property_value(hash_c1, DDS_AUTHTOKEN_PROP_HASH_C1, hash1_sent_in_request_arr, sizeof(hash1_sent_in_request_arr));
       }

       /* Set the DH public key associated with the local participant in dh1 property */
       if (diffie_hellman1) {
           set_binary_property_value(dh1, DDS_AUTHTOKEN_PROP_DH1, diffie_hellman1->data, diffie_hellman1->length);
       } else {
           set_binary_property_string(dh1, DDS_AUTHTOKEN_PROP_DH1 "x", "rubbish");
       }

       /* Set the challenge in challenge1 property */
       if (challengeData) {
           set_binary_property_value(challenge1, DDS_AUTHTOKEN_PROP_CHALLENGE1, challengeData, challengeDataSize);
       } else {
           set_binary_property_value(challenge1, DDS_AUTHTOKEN_PROP_CHALLENGE1 "x", challenge1_predefined_glb->value._buffer, challenge1_predefined_glb->value._length);
       }

       token->class_id = ddsrt_strdup(AUTH_HANDSHAKE_REQUEST_TOKEN_CLASS_ID);
       token->binary_properties._length = token->binary_properties._maximum = 8;
       token->binary_properties._buffer = tokens;
       break;

    case HANDSHAKE_REPLY:
        tokens = DDS_Security_BinaryPropertySeq_allocbuf(12);
        idx = 0;
        c_id = &tokens[idx++];
        c_perm = &tokens[idx++];
        c_pdata = &tokens[idx++];
        c_dsign_algo = &tokens[idx++];
        c_kagree_algo = &tokens[idx++];
        hash_c2 = &tokens[idx++];
        challenge2 = &tokens[idx++];
        dh2 = &tokens[idx++];
        challenge1 = &tokens[idx++];
        dh1 = &tokens[idx++];
        hash_c1 = &tokens[idx++] ;
        signature = &tokens[idx++];

        serializer_participant_data(pdata, &serialized_local_participant_data, &serialized_local_participant_data_size);

        /* Store the Identity Certificate associated with the local identify in c.id property */
        if (certificate) {
           set_binary_property_string(c_id, DDS_AUTHTOKEN_PROP_C_ID, certificate);
        } else {
           set_binary_property_string(c_id, DDS_AUTHTOKEN_PROP_C_ID "x", "rubbish");
        }

        /* Store the permission document in the c.perm property */
        set_binary_property_string(c_perm, DDS_AUTHTOKEN_PROP_C_PERM, "permissions_document");

        /* Store the provided local_participant_data in the c.pdata property */
        set_binary_property_value(c_pdata, DDS_AUTHTOKEN_PROP_C_PDATA, serialized_local_participant_data, (uint32_t)serialized_local_participant_data_size);
        ddsrt_free(serialized_local_participant_data);

        /* Set the used signing algorithm descriptor in c.dsign_algo */
        if (dsign) {
           set_binary_property_string(c_dsign_algo, DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO, dsign);
        } else {
           set_binary_property_string(c_dsign_algo, DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO "x", "rubbish");
        }

        /* Set the used key algorithm descriptor in c.kagree_algo */
        if (kagree) {
           set_binary_property_string(c_kagree_algo, DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO, kagree);
        } else {
           set_binary_property_string(c_kagree_algo, DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO "x", "rubbish");
        }

        CU_ASSERT_FATAL(hash1_from_request != NULL);
        assert(hash1_from_request != NULL); // for Clang's static analyzer

        set_binary_property_value(hash_c1, DDS_AUTHTOKEN_PROP_HASH_C1, hash1_from_request->value._buffer, hash1_from_request->value._length);

        /* Calculate the hash_c2 */
        {
           DDS_Security_BinaryPropertySeq bseq;
           DDS_Security_Serializer serializer;
           unsigned char hash2_sent_in_reply_arr[32];
           unsigned char *buffer;
           size_t size;

           bseq._length = bseq._maximum = 5;
           bseq._buffer = tokens;

           serializer = DDS_Security_Serializer_new(1024, 1024);

           DDS_Security_Serialize_BinaryPropertySeq(serializer, &bseq);
           DDS_Security_Serializer_buffer(serializer, &buffer, &size);
           SHA256(buffer, size, hash2_sent_in_reply_arr);

           ddsrt_free(buffer);
           DDS_Security_Serializer_free(serializer);

           set_binary_property_value(hash_c2, DDS_AUTHTOKEN_PROP_HASH_C2, hash2_sent_in_reply_arr, sizeof(hash2_sent_in_reply_arr));
        }

        /* Set the challenge in challenge1 property */
        if (challengeData) {
          set_binary_property_value(challenge1, DDS_AUTHTOKEN_PROP_CHALLENGE1, challengeData, challengeDataSize);
        } else {
          set_binary_property_value(challenge1, DDS_AUTHTOKEN_PROP_CHALLENGE1 "x", challenge2->value._buffer, challenge2->value._length);
        }

        /* Set the challenge in challenge2 property */
        if (challengeData2) {
           set_binary_property_value(challenge2, DDS_AUTHTOKEN_PROP_CHALLENGE2, challengeData2, challengeDataSize2);
        } else {
           set_binary_property_value(challenge2, DDS_AUTHTOKEN_PROP_CHALLENGE2 "x", challenge2->value._buffer, challenge2->value._length);
        }


        /* Set the DH public key associated with the local participant in dh1 property */
        if (diffie_hellman1) {
          set_binary_property_value(dh1, DDS_AUTHTOKEN_PROP_DH1, diffie_hellman1->data, diffie_hellman1->length);
        } else {
          set_binary_property_string(dh1, DDS_AUTHTOKEN_PROP_DH1 "x", "rubbish");
        }

        /* Set the DH public key associated with the local participant in dh2 property */
        if (diffie_hellman2) {
          set_binary_property_value(dh2, DDS_AUTHTOKEN_PROP_DH2, diffie_hellman2->data, diffie_hellman2->length);
        } else {
          set_binary_property_string(dh2, DDS_AUTHTOKEN_PROP_DH2 "x", "rubbish");
        }

        /* Calculate the signature */
        {
            BIO *bio;
            EVP_PKEY *private_key_x509;
            unsigned char *sign;
            size_t signlen;
            DDS_Security_ValidationResult_t rc;

            const DDS_Security_BinaryProperty_t * binary_properties[ HANDSHAKE_SIGNATURE_SIZE ];

            /* load certificate in buffer */
            bio = BIO_new_mem_buf((const char *) bob_private_key, -1);
            assert( bio );
            private_key_x509 = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
            assert (private_key_x509 );

            binary_properties[0] = hash_c2;
            binary_properties[1] = challenge2;
            binary_properties[2] = dh2;
            binary_properties[3] = challenge1;
            binary_properties[4] = dh1;
            binary_properties[5] = hash_c1;

            rc = create_signature_for_test(private_key_x509, binary_properties, HANDSHAKE_SIGNATURE_SIZE , &sign, &signlen, &exception);
            if (rc != DDS_SECURITY_VALIDATION_OK)
            {
                printf("Exception: %s\n", exception.message);
            }
            else
            {
                CU_ASSERT_FATAL (rc == DDS_SECURITY_VALIDATION_OK);
                assert(rc == DDS_SECURITY_VALIDATION_OK); // for Clang's static analyzer
                set_binary_property_value(signature, DDS_AUTHTOKEN_PROP_SIGNATURE, sign, (uint32_t)signlen);
                ddsrt_free(sign);
            }
            EVP_PKEY_free(private_key_x509);
            BIO_free(bio);
        }

        token->class_id = ddsrt_strdup(AUTH_HANDSHAKE_REPLY_TOKEN_CLASS_ID);
        token->binary_properties._length = token->binary_properties._maximum = 12;
        token->binary_properties._buffer = tokens;
        break;

    case HANDSHAKE_FINAL:
        tokens = DDS_Security_BinaryPropertySeq_allocbuf(7);
        idx = 0;
        signature = &tokens[idx++];
        hash_c1 = &tokens[idx++];
        challenge1 = &tokens[idx++];
        dh1 = &tokens[idx++];
        challenge2 = &tokens[idx++];
        dh2 = &tokens[idx++];
        hash_c2 = &tokens[idx++];

        CU_ASSERT(hash1_from_request != NULL);
        CU_ASSERT(hash2_from_reply != NULL);

        set_binary_property_value(hash_c1, DDS_AUTHTOKEN_PROP_HASH_C1, hash1_from_request->value._buffer, hash1_from_request->value._length);
        set_binary_property_value(hash_c2, DDS_AUTHTOKEN_PROP_HASH_C2, hash2_from_reply->value._buffer, hash2_from_reply->value._length);

        printf("process: %s\n", hash_c1->name);

        /* Set the challenge in challenge1 property */
        if (challengeData) {
          set_binary_property_value(challenge1, DDS_AUTHTOKEN_PROP_CHALLENGE1, challengeData, challengeDataSize);
        } else {
          set_binary_property_value(challenge1, DDS_AUTHTOKEN_PROP_CHALLENGE1 "x", challenge2->value._buffer, challenge2->value._length);
        }

        /* Set the challenge in challenge2 property */
        if (challengeData2) {
           set_binary_property_value(challenge2, DDS_AUTHTOKEN_PROP_CHALLENGE2, challengeData2, challengeDataSize2);
        } else {
           set_binary_property_value(challenge2, DDS_AUTHTOKEN_PROP_CHALLENGE2 "x", challenge2->value._buffer, challenge2->value._length);
        }


        /* Set the DH public key associated with the local participant in dh1 property */
        if (diffie_hellman1) {
          set_binary_property_value(dh1, DDS_AUTHTOKEN_PROP_DH1, diffie_hellman1->data, diffie_hellman1->length);
        } else {
          set_binary_property_string(dh1, DDS_AUTHTOKEN_PROP_DH1 "x", "rubbish");
        }

        /* Set the DH public key associated with the local participant in dh2 property */
        if (diffie_hellman2) {
          set_binary_property_value(dh2, DDS_AUTHTOKEN_PROP_DH2, diffie_hellman2->data, diffie_hellman2->length);
        } else {
          set_binary_property_string(dh2, DDS_AUTHTOKEN_PROP_DH2 "x", "rubbish");
        }

        /* Calculate the signature */
        {
            BIO *bio;
            EVP_PKEY *private_key_x509;
            unsigned char *sign;
            size_t signlen;
            const DDS_Security_BinaryProperty_t * binary_properties[ HANDSHAKE_SIGNATURE_SIZE ];

            /* load certificate in buffer */
            bio = BIO_new_mem_buf((const char *) bob_private_key, -1);
            assert( bio );
            private_key_x509 = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
            assert (private_key_x509 );

            binary_properties[0] = hash_c1;
            binary_properties[1] = challenge1;
            binary_properties[2] = dh1;
            binary_properties[3] = challenge2;
            binary_properties[4] = dh2;
            binary_properties[5] = hash_c2;

            if (create_signature_for_test(private_key_x509, binary_properties, HANDSHAKE_SIGNATURE_SIZE, &sign, &signlen, &exception) != DDS_SECURITY_VALIDATION_OK)
            {
                printf("Exception: %s\n", exception.message);
            }
            else
            {
                set_binary_property_value(signature, DDS_AUTHTOKEN_PROP_SIGNATURE, sign, (uint32_t)signlen);
                ddsrt_free(sign);
            }
            EVP_PKEY_free(private_key_x509);
            BIO_free(bio);
        }
        token->class_id = ddsrt_strdup(AUTH_HANDSHAKE_FINAL_TOKEN_CLASS_ID);
        token->binary_properties._length = token->binary_properties._maximum = 7;
        token->binary_properties._buffer = tokens;
        break;
    }
}


static DDS_Security_boolean on_revoke_identity_cb(const dds_security_authentication *plugin, const DDS_Security_IdentityHandle handle)
{
    DDSRT_UNUSED_ARG (plugin);
    if (identity_handle_for_callback1 == DDS_SECURITY_HANDLE_NIL)
        identity_handle_for_callback1 = handle;
    else if (identity_handle_for_callback2 == DDS_SECURITY_HANDLE_NIL)
        identity_handle_for_callback2 = handle;
    printf( "Listener called for handle: %lld  Local:%lld Remote:%lld\n", (long long) handle, (long long) local_identity_handle, (long long) remote_identity_handle2);

    return true;
}


/* sets listener for authentication expiry and checks if the listener triggered after a while */
CU_Test(ddssec_builtin_listeners_auth, local_remote_set_before_validation)
{
    DDS_Security_PermissionsHandle result;
    DDS_Security_PermissionsToken permissions_token;
    DDS_Security_AuthenticatedPeerCredentialToken credential_token;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_long valid;
    int r;
    dds_duration_t time_left =  DDS_MSECS(10000);
    DDS_Security_boolean local_expired = false;
    DDS_Security_boolean remote_expired = false;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_reply_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_out = DDS_SECURITY_TOKEN_INIT;
    const DDS_Security_BinaryProperty_t *hash1_sent_in_request;
    const DDS_Security_BinaryProperty_t *dh1;
    const DDS_Security_BinaryProperty_t *challenge1_glb;
    struct octet_seq dh1_pub_key;
    char *remote_certificate_file;

    local_expiry_date = DDS_TIME_INVALID;
    remote_expiry_date = DDS_TIME_INVALID;

    auth_listener.on_revoke_identity = &on_revoke_identity_cb;

    auth->set_listener( auth, &auth_listener, &exception);
    access_control->set_listener( access_control,
                        &access_control_listener,
                        &exception);


    valid = validate_local_identity_and_permissions( 3, &local_expiry_date);
    if (valid == DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE) {
        /* This can happen on very slow platforms or when doing a valgrind run.
         * Just take our losses and quit, simulating a success. */
        return;
    }
    CU_ASSERT_FATAL (valid == DDS_SECURITY_ERR_OK_CODE);

    /*Generate remote certificate*/
    create_certificate_from_csr( bob_csr, 4, BOB_IDENTITY_CERT_FILE, &remote_expiry_date );

    ddsrt_asprintf(&remote_certificate_file, "%s%s", g_path_build_dir, BOB_IDENTITY_CERT_FILE);
    bob_identity_cert = load_file_contents( remote_certificate_file );

    validate_remote_identity( bob_identity_cert );


    /* Check if we actually have validate_remote_permissions function. */
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL(access_control != NULL);
    assert(access_control != NULL);
    CU_ASSERT_FATAL(access_control->validate_remote_permissions != NULL);
    assert(access_control->validate_remote_permissions != 0);
    CU_ASSERT_FATAL(access_control->return_permissions_handle != NULL);
    assert(access_control->return_permissions_handle != 0);

    fill_permissions_token(&permissions_token);
    r = fill_peer_credential_token(&credential_token);
    CU_ASSERT_FATAL (r);



    /*handshake*/
    result = auth->begin_handshake_request(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    local_identity_handle,
                    remote_identity_handle2,
                    &serialized_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE) {
        printf("begin_handshake_request failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* get challenge 1 from the message */
    challenge1_glb = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_CHALLENGE1);

    /*Get DH1 value */
    dh1 = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_DH1);

    hash1_sent_in_request = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_HASH_C1);

    CU_ASSERT_FATAL(dh1 != NULL);
    assert(dh1 != NULL); // for Clang's static analyzer
    CU_ASSERT_FATAL(dh1->value._length > 0);
    CU_ASSERT_FATAL(dh1->value._buffer != NULL);
    assert(dh1->value._length > 0 && dh1->value._buffer != NULL); // for Clang's static analyzer

    dh1_pub_key.data = dh1->value._buffer;
    dh1_pub_key.length = dh1->value._length;

    /* prepare reply */
    fill_handshake_message_token(
        &handshake_reply_token_in, remote_participant_data2, bob_identity_cert,
        AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_ECDH_NAME,
        &dh1_pub_key, challenge1_glb->value._buffer, challenge1_glb->value._length,
        &dh_ecdh_pub_key, challenge2_predefined_glb->value._buffer, challenge2_predefined_glb->value._length, hash1_sent_in_request, NULL, HANDSHAKE_REPLY);

    reset_exception(&exception);

    result = auth->process_handshake(
                        auth,
                        &handshake_reply_token_out,
                        &handshake_reply_token_in,
                        handshake_handle,
                        &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE);
    CU_ASSERT(handshake_handle != DDS_SECURITY_HANDLE_NIL);

    result = access_control->validate_remote_permissions(
                access_control,
                auth,
                local_identity_handle,
                remote_identity_handle,
                &permissions_token,
                &credential_token,
                &exception);

    if (result == 0) {
        printf("validate_remote_permissions_failed: %s\n", exception.message ? exception.message : "Error message missing");
        //TODO: Clean-up before failing
        CU_ASSERT_FATAL (exception.code == DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE);
//        goto end;

    }

    remote_permissions_handle = result;

    reset_exception(&exception);

    while( time_left > 0 && (!local_expired || !remote_expired) ){
        /* Normally, it is expected that the remote expiry is triggered before the
         * local one. However, that can change on slow platforms. */
        // TODO: Check for time compare
        if (remote_expiry_date < local_expiry_date) {
            if (identity_handle_for_callback1 == remote_identity_handle2) {
                remote_expired = true;
            }
            if (identity_handle_for_callback2 == local_identity_handle) {
                local_expired = true;
            }
        } else {
            if (identity_handle_for_callback2 == remote_identity_handle2) {
                remote_expired = true;
            }
            if (identity_handle_for_callback1 == local_identity_handle) {
                local_expired = true;
            }
        }

        dds_sleepfor(DDS_MSECS(100));
        time_left -= DDS_MSECS(100);
    }

    CU_ASSERT (local_expired);
    CU_ASSERT (remote_expired);


    reset_exception(&exception);

    result = auth->return_handshake_handle(auth, handshake_handle, &exception);
    release_remote_identities();

    DDS_Security_DataHolder_deinit((DDS_Security_DataHolder *) &permissions_token);
    DDS_Security_DataHolder_deinit((DDS_Security_DataHolder *) &credential_token);


    CU_ASSERT_TRUE (result == 1);

    reset_exception(&exception);

    DDS_Security_DataHolder_deinit(&handshake_token_out);
    DDS_Security_DataHolder_deinit(&handshake_reply_token_in);
    DDS_Security_DataHolder_deinit(&handshake_reply_token_out);

    clear_local_identity_and_permissions();
    ddsrt_free( bob_identity_cert );
    ddsrt_free ( remote_certificate_file );
}
