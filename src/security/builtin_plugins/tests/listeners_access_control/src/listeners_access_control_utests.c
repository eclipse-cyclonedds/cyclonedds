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
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__xevent.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/openssl_support.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "common/src/loader.h"
#include "config_env.h"
#include "auth_tokens.h"
#include "ac_tokens.h"

static const char *SUBJECT_NAME_PERMISSIONS_CA = "C=NL, ST=Some-State, O=ADLINK Technolocy Inc., CN=adlinktech.com";
static const char *RSA_2048_ALGORITHM_NAME = "RSA-2048";

static const char *RELATIVE_PATH_TO_ETC_DIR = "/listeners_access_control/etc/";
static const char *PERMISSIONS_CA_CERT_FILE = "Test_Permissions_ca.pem";
static const char *PERMISSIONS_CA_KEY_FILE = "Test_Permissions_ca_key.pem";
static const char *PERMISSIONS_FILE = "Test_Permissions_listener.p7s";
static dds_security_access_control_listener ac_listener;

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

static const char *permissions_ca = /*Test_Permissions_ca.pem */
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

#define PERMISSIONS_DOCUMENT "<?xml version=\"1.0\" encoding=\"utf-8\"?>                                    \
<dds xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"                                                            \
    xsi:noNamespaceSchemaLocation=\"https://www.omg.org/spec/DDS-SECURITY/20170901/omg_shared_ca_permissions.xsd\">      \
    <permissions>                                                                                                       \
        <grant name=\"DEFAULT_PERMISSIONS\">                                                                            \
            <subject_name>/C=NL/ST=Some-State/O=Internet Widgits Pty Ltd/CN=CHAM500 cert</subject_name>                                                                               \
            <validity>                                                                                                  \
                <not_before>2015-09-15T01:00:00</not_before>                                                            \
                <not_after>PERMISSION_EXPIRY_DATE</not_after>                                                              \
            </validity>                                                                                                 \
            <deny_rule>                                                                                                \
                <domains>                                                                                               \
                    <id_range>                                                                                          \
                        <min>0</min>                                                                                    \
                        <max>230</max>                                                                                  \
                    </id_range>                                                                                         \
                </domains>                                                                                              \
                <publish>                                                                                               \
                    <topics>                                                                                            \
                        <topic>*</topic>                                                                                \
                    </topics>                                                                                           \
                    <partitions/>                                                                                       \
                </publish>                                                                                              \
                <subscribe>                                                                                             \
                    <topics>                                                                                            \
                        <topic>*</topic>                                                                                \
                    </topics>                                                                                           \
                    <partitions/>                                                                                       \
                </subscribe>                                                                                            \
           </deny_rule>                                                                                                \
           <default>DENY</default>                                                                                     \
        </grant>                                                                                                        \
    </permissions>                                                                                                      \
</dds>                                  "

static struct plugins_hdl *plugins = NULL;
static dds_security_authentication *auth = NULL;
static dds_security_access_control *access_control = NULL;
static DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_PermissionsHandle local_permissions_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_PermissionsHandle remote_permissions_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_GUID_t local_participant_guid;
static char *g_path_to_etc_dir = NULL;
static char *g_path_build_dir = NULL;
static DDS_Security_PermissionsHandle permission_handle_for_callback1 = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_PermissionsHandle permission_handle_for_callback2 = DDS_SECURITY_HANDLE_NIL;
static dds_time_t local_expiry_date;
static dds_time_t remote_expiry_date;

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

static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
}

static void get_future_xsdate(char *str, size_t len, int32_t delta)
{
  time_t rawtime;
  struct tm *future = ddsrt_malloc(sizeof(struct tm));

  /* Get future time. */
  rawtime = time(NULL) + delta;
  OPENSSL_gmtime(&rawtime, future);

  /* Put the future time in a xsDate format. */
  strftime(str, len, "%Y-%m-%dT%H:%M:%S", future);

  ddsrt_free(future);
}

static int smime_sign(const char *certificate_file, const char *key_file, const char *data, const char *out_file)
{
  BIO *in = NULL, *out = NULL, *tbio = NULL, *keybio = NULL;
  X509 *scert = NULL;
  EVP_PKEY *skey = NULL;
  PKCS7 *p7 = NULL;
  int ret = 1;
  int flags = PKCS7_DETACHED | PKCS7_STREAM | PKCS7_TEXT;

  /* Read in signer certificate and private key */
  tbio = BIO_new_file(certificate_file, "r");
  if (!tbio)
    goto err;
  scert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

  keybio = BIO_new_file(key_file, "r");
  if (!keybio)
    goto err;

  skey = PEM_read_bio_PrivateKey(keybio, NULL, 0, NULL);
  if (!scert || !skey)
    goto err;

  /* Open content being signed */
  in = BIO_new_mem_buf(data, (int)strlen(data));
  if (!in)
    goto err;
  /* Sign content */
  p7 = PKCS7_sign(scert, skey, NULL, in, flags);
  if (!p7)
    goto err;
  out = BIO_new_file(out_file, "w");
  if (!out)
    goto err;

  //if (!(flags & PKCS7_STREAM))
  //    BIO_reset(in);

  /* Write out S/MIME message */
  if (!SMIME_write_PKCS7(out, p7, in, flags))
    goto err;
  ret = 0;
err:
  if (ret)
  {
    fprintf(stderr, "Error Signing Data\n");
    ERR_print_errors_fp(stderr);
  }
  if (p7)
    PKCS7_free(p7);
  if (scert)
    X509_free(scert);
  if (skey)
    EVP_PKEY_free(skey);
  if (in)
    BIO_free(in);
  if (keybio)
    BIO_free(keybio);
  if (out)
    BIO_free(out);
  if (tbio)
    BIO_free(tbio);

  return ret;
}

static void fill_participant_qos(DDS_Security_Qos *qos, int32_t permission_expiry, const char *governance_filename)
{
  char *permission_uri;
  char *governance_uri;
  char *permissions_ca_cert_file;
  char *permissions_ca_key_file;
  char *permissions_file;
  char *permissions_xml_with_expiry;
  char permission_expiry_date_str[30];

  /*get time in future */
  get_future_xsdate(permission_expiry_date_str, 30, permission_expiry);
  local_expiry_date = DDS_Security_parse_xml_date(permission_expiry_date_str);

  permissions_xml_with_expiry = ddsrt_str_replace(PERMISSIONS_DOCUMENT, "PERMISSION_EXPIRY_DATE", permission_expiry_date_str, 1);

  ddsrt_asprintf(&permissions_ca_cert_file, "%s%s", g_path_to_etc_dir, PERMISSIONS_CA_CERT_FILE);
  ddsrt_asprintf(&permissions_ca_key_file, "%s%s", g_path_to_etc_dir, PERMISSIONS_CA_KEY_FILE);
  ddsrt_asprintf(&permissions_file, "%s%s", g_path_build_dir, PERMISSIONS_FILE);

  smime_sign(permissions_ca_cert_file, permissions_ca_key_file, permissions_xml_with_expiry, permissions_file);

  //check sign result
  ddsrt_asprintf(&permission_uri, "file:%s", permissions_file);
  ddsrt_asprintf(&governance_uri, "file:%s%s", g_path_to_etc_dir, governance_filename);

  memset(qos, 0, sizeof(*qos));
  dds_security_property_init(&qos->property.value, 6);
  qos->property.value._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
  qos->property.value._buffer[0].value = ddsrt_strdup(identity_certificate);
  qos->property.value._buffer[1].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
  qos->property.value._buffer[1].value = ddsrt_strdup(identity_ca);
  qos->property.value._buffer[2].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
  qos->property.value._buffer[2].value = ddsrt_strdup(private_key);
  qos->property.value._buffer[3].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_PERMISSIONS_CA);
  qos->property.value._buffer[3].value = ddsrt_strdup(permissions_ca);
  qos->property.value._buffer[4].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_PERMISSIONS);
  qos->property.value._buffer[4].value = ddsrt_strdup(permission_uri);
  qos->property.value._buffer[5].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_GOVERNANCE);
  qos->property.value._buffer[5].value = ddsrt_strdup(governance_uri);

  ddsrt_free(permission_uri);
  ddsrt_free(governance_uri);
  ddsrt_free(permissions_xml_with_expiry);
  ddsrt_free(permissions_ca_key_file);
  ddsrt_free(permissions_ca_cert_file);
  ddsrt_free(permissions_file);
}

static void fill_permissions_token(DDS_Security_PermissionsToken *token)
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

static int fill_peer_credential_token(DDS_Security_AuthenticatedPeerCredentialToken *token, int32_t permission_expiry)
{
  int result = 1;
  char *permission_data;

  char *permissions_ca_cert_file;
  char *permissions_ca_key_file;
  char *permissions_file;
  char *permissions_xml_with_expiry;
  char permission_expiry_date_str[30];

  /*get time in future */
  get_future_xsdate(permission_expiry_date_str, 30, permission_expiry);
  remote_expiry_date = DDS_Security_parse_xml_date(permission_expiry_date_str);
  permissions_xml_with_expiry = ddsrt_str_replace(PERMISSIONS_DOCUMENT, "PERMISSION_EXPIRY_DATE", permission_expiry_date_str, 1);

  ddsrt_asprintf(&permissions_ca_cert_file, "%s%s", g_path_to_etc_dir, PERMISSIONS_CA_CERT_FILE);
  ddsrt_asprintf(&permissions_ca_key_file, "%s%s", g_path_to_etc_dir, PERMISSIONS_CA_KEY_FILE);
  ddsrt_asprintf(&permissions_file, "%s%s", g_path_build_dir, PERMISSIONS_FILE);

  smime_sign(permissions_ca_cert_file, permissions_ca_key_file, permissions_xml_with_expiry, permissions_file);

  memset(token, 0, sizeof(DDS_Security_AuthenticatedPeerCredentialToken));

  permission_data = load_file_contents(permissions_file);

  if (permission_data)
  {
    token->class_id = ddsrt_strdup(DDS_AUTHTOKEN_CLASS_ID);
    token->properties._length = token->properties._maximum = 2;
    token->properties._buffer = DDS_Security_PropertySeq_allocbuf(2);

    token->properties._buffer[0].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_C_ID);
    token->properties._buffer[0].value = ddsrt_strdup(&identity_certificate[6]);

    token->properties._buffer[1].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_C_PERM);
    token->properties._buffer[1].value = permission_data;
  }
  else
  {
    ddsrt_free(permission_data);
    result = 0;
  }

  ddsrt_free(permissions_xml_with_expiry);
  ddsrt_free(permissions_ca_key_file);
  ddsrt_free(permissions_ca_cert_file);
  ddsrt_free(permissions_file);
  return result;
}

static DDS_Security_long
validate_local_identity_and_permissions(int32_t permission_expiry)
{
  DDS_Security_long res = DDS_SECURITY_ERR_OK_CODE;
  DDS_Security_ValidationResult_t result;
  DDS_Security_DomainId domain_id = 0;
  DDS_Security_Qos participant_qos;
  DDS_Security_GUID_t candidate_participant_guid;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_GuidPrefix_t prefix = {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb};
  DDS_Security_EntityId_t entityId = {{0xb0, 0xb1, 0xb2}, 0x1};

  memset(&local_participant_guid, 0, sizeof(local_participant_guid));
  memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
  memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

  fill_participant_qos(&participant_qos, permission_expiry, "Test_Governance_ok.p7s");

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
    res = DDS_SECURITY_ERR_UNDEFINED_CODE;
    printf("validate_local_identity_failed: (%d) %s\n", (int)exception.code, exception.message ? exception.message : "Error message missing");
  }

  reset_exception(&exception);

  if (res == 0)
  {
    local_permissions_handle = access_control->validate_local_permissions(
        access_control,
        auth,
        local_identity_handle,
        0,
        &participant_qos,
        &exception);

    if (local_permissions_handle == DDS_SECURITY_HANDLE_NIL)
    {
      printf("validate_local_permissions_failed: (%d) %s\n", (int)exception.code, exception.message ? exception.message : "Error message missing");
      if (exception.code == DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE)
        /* This can happen on very slow platforms or when doing a valgrind run. */
        res = DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE;
      else
        res = DDS_SECURITY_ERR_UNDEFINED_CODE;
    }
  }

  dds_security_property_deinit(&participant_qos.property.value);
  ddsrt_free(exception.message);

  return res;
}

static void clear_local_identity_and_permissions(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_boolean success;

  if (local_permissions_handle != DDS_SECURITY_HANDLE_NIL)
  {
    success = access_control->return_permissions_handle(access_control, local_permissions_handle, &exception);
    if (!success)
      printf("return_permission_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    reset_exception(&exception);
  }

  if (local_identity_handle != DDS_SECURITY_HANDLE_NIL)
  {
    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    if (!success)
      printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    reset_exception(&exception);
  }
}

static void set_path_to_etc_dir(void)
{
  ddsrt_asprintf(&g_path_to_etc_dir, "%s%s", CONFIG_ENV_TESTS_DIR, RELATIVE_PATH_TO_ETC_DIR);
}
static void set_path_build_dir(void)
{
  ddsrt_asprintf(&g_path_build_dir, "%s/", CONFIG_ENV_BUILD_DIR);
}

CU_Init(ddssec_builtin_listeners_access_control)
{
  int res = 0;
  plugins = load_plugins(&access_control, &auth, NULL /* Cryptograpy */,
                         &(const struct ddsi_domaingv){ .handshake_include_optional = true });
  if (!plugins) {
    res = -1;
  } else {
    set_path_to_etc_dir();
    set_path_build_dir();
    dds_openssl_init ();
  }

  return res;
}

CU_Clean(ddssec_builtin_listeners_access_control)
{
  unload_plugins(plugins);
  ddsrt_free(g_path_to_etc_dir);
  return 0;
}

static DDS_Security_boolean on_revoke_permissions_cb(const dds_security_access_control *plugin, const DDS_Security_PermissionsHandle handle)
{
  DDSRT_UNUSED_ARG(plugin);
  if (permission_handle_for_callback1 == DDS_SECURITY_HANDLE_NIL)
    permission_handle_for_callback1 = handle;
  else if (permission_handle_for_callback2 == DDS_SECURITY_HANDLE_NIL)
    permission_handle_for_callback2 = handle;
  printf("Listener called for handle: %lld  Local:%lld Remote:%lld\n", (long long)handle, (long long)local_permissions_handle, (long long)remote_permissions_handle);
  return true;
}

CU_Test(ddssec_builtin_listeners_access_control, local_2secs)
{
  DDS_Security_PermissionsHandle result;
  DDS_Security_PermissionsToken permissions_token;
  DDS_Security_AuthenticatedPeerCredentialToken credential_token;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_long valid;
  int r;
  dds_duration_t time_left = DDS_MSECS(10000);
  bool local_expired = false;
  bool remote_expired = false;

  local_expiry_date = 0;
  remote_expiry_date = 0;

  ac_listener.on_revoke_permissions = &on_revoke_permissions_cb;

  valid = validate_local_identity_and_permissions(2);
  if (valid == DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE)
  {
    /* This can happen on very slow platforms or when doing a valgrind run.
         * Just take our losses and quit, simulating a success. */
    return;
  }
  CU_ASSERT_FATAL(valid == DDS_SECURITY_ERR_OK_CODE);

  /* Check if we actually have validate_remote_permissions function. */
  CU_ASSERT_FATAL(local_identity_handle != DDS_SECURITY_HANDLE_NIL);
  CU_ASSERT_FATAL(access_control != NULL);
  assert(access_control != NULL);
  CU_ASSERT_FATAL(access_control->validate_remote_permissions != NULL);
  assert(access_control->validate_remote_permissions != 0);
  CU_ASSERT_FATAL(access_control->return_permissions_handle != NULL);
  assert(access_control->return_permissions_handle != 0);

  fill_permissions_token(&permissions_token);
  r = fill_peer_credential_token(&credential_token, 1);
  CU_ASSERT_FATAL(r);

  remote_identity_handle++;

  access_control->set_listener(access_control, &ac_listener, &exception);

  result = access_control->validate_remote_permissions(
      access_control,
      auth,
      local_identity_handle,
      remote_identity_handle,
      &permissions_token,
      &credential_token,
      &exception);

  if (result == 0)
  {
    printf("validate_remote_permissions_failed: %s\n", exception.message ? exception.message : "Error message missing");
    /* Expiry can happen on very slow platforms or when doing a valgrind run.
         * Just take our losses and quit, simulating a success. */
    CU_ASSERT(exception.code == DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE);
    goto end;
  }

  remote_permissions_handle = result;

  reset_exception(&exception);

  while (time_left > 0 && (!local_expired || !remote_expired))
  {
    /* Normally, it is expected that the remote expiry is triggered before the
         * local one. However, that can change on slow platforms. */
    if (remote_expiry_date < local_expiry_date)
    {
      if (permission_handle_for_callback1 == remote_permissions_handle)
      {
        remote_expired = true;
      }
      if (permission_handle_for_callback2 == local_permissions_handle)
      {
        local_expired = true;
      }
    }
    else
    {
      if (permission_handle_for_callback2 == remote_permissions_handle)
      {
        remote_expired = true;
      }
      if (permission_handle_for_callback1 == local_permissions_handle)
      {
        local_expired = true;
      }
    }

    dds_sleepfor(DDS_MSECS(100));
    time_left -= DDS_MSECS(100);
  }

  CU_ASSERT(local_expired);
  CU_ASSERT(remote_expired);

  access_control->return_permissions_handle(access_control, result, &exception);

end:
  reset_exception(&exception);

  DDS_Security_DataHolder_deinit((DDS_Security_DataHolder *)&permissions_token);
  DDS_Security_DataHolder_deinit((DDS_Security_DataHolder *)&credential_token);

  clear_local_identity_and_permissions();
}
