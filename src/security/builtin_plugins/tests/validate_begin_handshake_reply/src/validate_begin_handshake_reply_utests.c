#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/environ.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/openssl_support.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "common/src/loader.h"
#include "config_env.h"
#include "auth_tokens.h"
#include "ac_tokens.h"

static const char * SUBJECT_NAME_IDENTITY_CERT      = "CN=CHAM-574 client,O=Some Company,ST=Some-State,C=NL";
static const char * SUBJECT_NAME_IDENTITY_CA        = "CN=CHAM-574 authority,O=Some Company,ST=Some-State,C=NL";

static const char * RSA_2048_ALGORITHM_NAME         = "RSA-2048";

static const char * AUTH_HANDSHAKE_REQUEST_TOKEN_CLASS_ID = "DDS:Auth:PKI-DH:1.0+Req";
static const char * AUTH_HANDSHAKE_REPLY_TOKEN_CLASS_ID   = "DDS:Auth:PKI-DH:1.0+Reply";


static const char * AUTH_DSIGN_ALGO_RSA_NAME  = "RSASSA-PSS-SHA256";
static const char * AUTH_KAGREE_ALGO_RSA_NAME = "DH+MODP-2048-256";

static unsigned char *dh_pubkey_modp_2048_value          = NULL;
static unsigned char *invalid_dh_pubkey_modp_2048_value  = NULL;
static size_t  dh_pubkey_modp_2048_length         = 0;

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


static const char *remote_identity_certificate =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIEPTCCAyWgAwIBAgIIRmtzSKaI+rowDQYJKoZIhvcNAQELBQAwXzELMAkGA1UE\n"
        "BhMCTkwxEzARBgNVBAgTClNvbWUtU3RhdGUxITAfBgNVBAoTGEludGVybmV0IFdp\n"
        "ZGdpdHMgUHR5IEx0ZDEYMBYGA1UEAxMPQ0hBTTUwMCByb290IGNhMCAXDTE4MDMy\n"
        "MzEyMDEwMFoYDzIyMjIwMjIyMjIyMjAwWjBYMQswCQYDVQQGEwJOTDETMBEGA1UE\n"
        "CBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRk\n"
        "MREwDwYDVQQDEwhDSEFNLTU3NzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoC\n"
        "ggEBAMKlWGK8f/AEjK7viu9CdydJmorKyk3oK8PWmZX3B+3k8eFW32NXv2+BK5vk\n"
        "sAOhEcCAV2/125iAGvXs5vq5GshjgHXwJysKOFBwLiCPHzLaOX095ib6pgPejjgV\n"
        "gGsGrRAKetCAqxv+pf1n4zD9VSLDrnHrxbzvosQdBCgSBPiTFK5qDAGhVGR48Pp9\n"
        "gAqZPhdLfH47S/6scRJNywoXrIxp2CnuHd4fVvyQlPNLHwlX1nOr76bGOjGqFsFU\n"
        "/mcPN7aGFIh4KQK9KvHt5+SApLgBBdrn9njgaIC7VN9ddSp2Jz2vHAPR52dqM0SW\n"
        "dl7uyOiT/TK6q8f7aFKqk29r/OkCAwEAAaOCAQAwgf0wDAYDVR0PBAUDAwf/gDCB\n"
        "7AYDVR0lBIHkMIHhBggrBgEFBQcDAQYIKwYBBQUHAwIGCCsGAQUFBwMDBggrBgEF\n"
        "BQcDBAYIKwYBBQUHAwgGCisGAQQBgjcCARUGCisGAQQBgjcCARYGCisGAQQBgjcK\n"
        "AwEGCisGAQQBgjcKAwMGCisGAQQBgjcKAwQGCWCGSAGG+EIEAQYLKwYBBAGCNwoD\n"
        "BAEGCCsGAQUFBwMFBggrBgEFBQcDBgYIKwYBBQUHAwcGCCsGAQUFCAICBgorBgEE\n"
        "AYI3FAICBggrBgEFBQcDCQYIKwYBBQUHAw0GCCsGAQUFBwMOBgcrBgEFAgMFMA0G\n"
        "CSqGSIb3DQEBCwUAA4IBAQAniERWU9f/ijm9t8xuyOujEKDJl0Ded4El9mM5UYPR\n"
        "ZSnabPNKQjABBS4sVISIYVwfQxGkPgK0MeMBKqs/kWsZ4rp8h5hlZvxFX8H148mo\n"
        "3apNgdc/VylDBW5Ltbrypn/dZh9hFZE8Y/Uvo9HPksVEkjYuFN5v7e8/mwxTcrZ1\n"
        "BAZrTlDTiCR046NN1lUs/7oUaNCruFV7AU6RbGYnSzM6plPJHMRa9nzNeO0uPaHK\n"
        "kNPe+/UGpMi7cpF9w0M5Z1wW+Nq45bBRejFLQkHSjOEeGL2zi7T1HFAHZQydd6Wo\n"
        "zYffGTmyHqIjNArbOWEMYN6s1nqsQS+ifolr0MtfeHad\n"
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


static const char *diffie_hellman_pubkey =
        "-----BEGIN PUBLIC KEY-----\n"
        "MIICJDCCARcGCSqGSIb3DQEDATCCAQgCggEBAJxLLigSIC7JjO/kdQ7LT1v0FPvM\n"
        "Cq4hZrg6cX1IKzinaJJai3CcWpjMxoJ+jDBh2iekZavJli9qa400FC94UchqsE8s\n"
        "I68LRbSkocvSWHOyayGrn04XLvhZPwulZFfEZ0xmg+vcTt2xS5IvX0qC1nrMpVu9\n"
        "d9A93n+PZqRWJcJThoApChRT0lVp6CC9dn7oVnIPWyGfUZH9UOzXrx8Rq6wKnWPs\n"
        "G0Igaia4uLDIqGKJ1Nr2HmLY8exKcK539X38I2NokzQrwhKdBlIXF5yYGhH+ib0U\n"
        "yrdbK1kJBiaXh2CMMPQ7skXZDeQ/ixgTI/cXwaMC3ddE+7l/GTHo/azJawsCAQID\n"
        "ggEFAAKCAQB3SB4wP5voPc5YcUpFiJXXlQe7DQxfzo5Mq1A/e8Raw/qzCkJMkcoT\n"
        "v656vj4s7PbmzWLDLs0mAD7lU4U+HSnhuBmP46aIZVZORZqQmhzn073iqPiRN8eC\n"
        "XPXgsMc8sgpbOoUGo89nuMGaucu2i4ZpLdJTkoFfC6wE2wZg11mr/hfX7+KmKSSp\n"
        "V/+h6wROt824MijsuDjxHgZJWuM1jqzFq5skMJ84uwBF5LG3A6sTFnobBQXily5H\n"
        "vYh/wWf+lRxeoNRW6B0t7xukZ+a71gg2Fxtm1f3RkLh4IcWfuYAcn5R9Hvgvx7Ex\n"
        "DrqVRGbTZZa7fgtiQjj7HF6Cg/btOz2T\n"
        "-----END PUBLIC KEY-----\n";

static const char *unrelated_identity =
        "data:,-----BEGIN CERTIFICATE-----\n"
        "MIIDpDCCAoygAwIBAgIJALE5lRKfYHAaMA0GCSqGSIb3DQEBCwUAMF8xCzAJBgNV\n"
        "BAYTAk5MMRMwEQYDVQQIDApPdmVyaWpzc2VsMRAwDgYDVQQHDAdIZW5nZWxvMQ8w\n"
        "DQYDVQQKDAZBRExJTksxGDAWBgNVBAMMD0NIQU01MDAgUm9vdCBDQTAeFw0xODAy\n"
        "MDkxNjIwNDNaFw0zODAyMDQxNjIwNDNaMF8xCzAJBgNVBAYTAk5MMRMwEQYDVQQI\n"
        "DApPdmVyaWpzc2VsMRAwDgYDVQQHDAdIZW5nZWxvMQ8wDQYDVQQKDAZBRExJTksx\n"
        "GDAWBgNVBAMMD0NIQU01MDAgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEP\n"
        "ADCCAQoCggEBAN9/NbpJDHQYHh3cEByRxnHffxEe9Sapn08Ty5xYO8LDJ4V7vU32\n"
        "/7291fITiHaovOoCRHAbKTaTtqJO56aGY45HON6KIqxljLQJJVGW/Nf2PNSHmFix\n"
        "6D6bsoSOTPyKYqBNT6lB7NMn4QBTcsiE61El8p9WLQZHoYQJK5Psf7wkBqGBz8he\n"
        "bcDWXFn7kIgnsaLrh77w2wi/y0MqpPwyeRInoZfYknzVNdxCPgq7csBYDoMgOgkV\n"
        "G60ECXojHKz1HI4n0V8L8lZluSSVRNR0xvPFgBqO7b+Re7xb6iO9TNsFeoiMMNyp\n"
        "EwM99CqPO0RRrAPiC7IDgcNGjxhne9EJFGsCAwEAAaNjMGEwHQYDVR0OBBYEFCst\n"
        "gj5Ecm3HU/N7wxJluFo5+6XUMB8GA1UdIwQYMBaAFCstgj5Ecm3HU/N7wxJluFo5\n"
        "+6XUMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/BAQDAgGGMA0GCSqGSIb3DQEB\n"
        "CwUAA4IBAQCWibvYuPLpoNcsUdHbE7SnBbEQnDfBxBZN8xeWHwwAPEB+8eHhmIdZ\n"
        "xDtCN61xr5QR+KzlEYFwKyHMp9GN3OPU1RndJrzaXz2ddAZVkBIvnQZ4JvFd+sBC\n"
        "QQgEvL8GcwZPxnad/TRylM4ON3Kh0X9vfyrmWEoHephiE1LcENaFqcYr9xg3DJNh\n"
        "XSrigMGZJ7IOHkvgaoneICOcYI42ZHS0fnt1G+01VKJXm3ndi5NL25GnOmlvV6yV\n"
        "+1vcmdQc6YS8K8vHmrH4lX9iPfsOak6WSzzsXdqgpvyxtGJggcFaDTtmbWCAkJj0\n"
        "B7DMeaVlLClGQaKZZ7aexEx9se+IyLn2\n"
        "-----END CERTIFICATE-----\n";


static const char *remote_identity_trusted =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDcDCCAligAwIBAgIBBTANBgkqhkiG9w0BAQsFADByMQswCQYDVQQGEwJOTDEL\n"
        "MAkGA1UECBMCT1YxEzARBgNVBAoTCkFETGluayBJU1QxGTAXBgNVBAMTEElkZW50\n"
        "aXR5IENBIFRlc3QxJjAkBgkqhkiG9w0BCQEWF2luZm9AaXN0LmFkbGlua3RlY2gu\n"
        "Y29tMB4XDTE4MDMxMjAwMDAwMFoXDTI3MDMxMTIzNTk1OVowcDELMAkGA1UEBhMC\n"
        "TkwxCzAJBgNVBAgTAk9WMQ8wDQYDVQQKEwZBRExpbmsxETAPBgNVBAsTCElTVCBU\n"
        "ZXN0MREwDwYDVQQDEwhCb2IgVGVzdDEdMBsGCSqGSIb3DQEJARYOYm9iQGFkbGlu\n"
        "ay5pc3QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDB5vqhuWnwhxXZ\n"
        "qffPmfjzge7w91oX4ISlboIfBXp3sLj2mqLSsYhgBNJAn/Fl1OZeFw0d4gVibBgx\n"
        "5Zdcjsi+ClvYK8H534iTJfNriMyhg4kSWxZF1Tixfw3FS7LqjKEY5ZNPfp5a4P+8\n"
        "UveorYJusrnlv1DiF6aPhJQh8J62J6bhx62DNLO7dZbN0BUsnWtyDcfi5DOjf2/r\n"
        "3lSRfecn3uBr1QYRaS5FrV+MSoGcjI3M75mei1TTUp7YT4ZWRR5rKUMql605xsms\n"
        "d6sqJaKofYmw7wCuaVJ86pb/w8srdddKt21xUeQNMKn49H6raezMOE3U5BUMtZ+P\n"
        "2OBLk/CPAgMBAAGjEzARMA8GA1UdDwEB/wQFAwMH/4AwDQYJKoZIhvcNAQELBQAD\n"
        "ggEBAJV71Ckf1zsks5mJXqdUb8bTVHg4hN32pwjCL5c6W2XHAv+YHwE/fN3C1VIY\n"
        "bC8zjUC9dCOyC2AvOQyZQ1eC/WoK6FlXjHVX2upL4lXQ9WL9ztt1mgdRrhvUPuUn\n"
        "aBE8VgNU0t4jl93xMIaU8hB0kQsV+kdcN0cWbrF3mT4s9njRvopJ8hS2UE60V2wA\n"
        "ceUOazH+QGPh1k0jkynrTlVR9GrpebQwZ2UFeinVO0km17IAyQkz+OmPc4jQLJMl\n"
        "CmkbmMwowdLMKC6r/HyE87dN7NvFnRM5iByJklRwN7WDYZrl72HoUOlgTZ7PjW2G\n"
        "jTxK8xXtDCXC/3CNpe0YFnOga8g=\n"
        "-----END CERTIFICATE-----\n";

static const char *remote_identity_untrusted =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIELTCCAxWgAwIBAgIBATANBgkqhkiG9w0BAQsFADB0MQswCQYDVQQGEwJOTDEL\n"
        "MAkGA1UECBMCT1YxEzARBgNVBAoTCkFETGluayBJU1QxGzAZBgNVBAMTEkJvYiBU\n"
        "ZXN0IFVudHJ1c3RlZDEmMCQGCSqGSIb3DQEJARYXaW5mb0Bpc3QuYWRsaW5rdGVj\n"
        "aC5jb20wHhcNMTgwNjIwMDAwMDAwWhcNMjcwNjE5MjM1OTU5WjB0MQswCQYDVQQG\n"
        "EwJOTDELMAkGA1UECBMCT1YxEzARBgNVBAoTCkFETGluayBJU1QxGzAZBgNVBAMT\n"
        "EkJvYiBUZXN0IFVudHJ1c3RlZDEmMCQGCSqGSIb3DQEJARYXaW5mb0Bpc3QuYWRs\n"
        "aW5rdGVjaC5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDB5vqh\n"
        "uWnwhxXZqffPmfjzge7w91oX4ISlboIfBXp3sLj2mqLSsYhgBNJAn/Fl1OZeFw0d\n"
        "4gVibBgx5Zdcjsi+ClvYK8H534iTJfNriMyhg4kSWxZF1Tixfw3FS7LqjKEY5ZNP\n"
        "fp5a4P+8UveorYJusrnlv1DiF6aPhJQh8J62J6bhx62DNLO7dZbN0BUsnWtyDcfi\n"
        "5DOjf2/r3lSRfecn3uBr1QYRaS5FrV+MSoGcjI3M75mei1TTUp7YT4ZWRR5rKUMq\n"
        "l605xsmsd6sqJaKofYmw7wCuaVJ86pb/w8srdddKt21xUeQNMKn49H6raezMOE3U\n"
        "5BUMtZ+P2OBLk/CPAgMBAAGjgckwgcYwDAYDVR0TBAUwAwEB/zAdBgNVHQ4EFgQU\n"
        "QpxLPHT5o/GQRwdBw2scINXnWlUwHwYDVR0jBBgwFoAUQpxLPHT5o/GQRwdBw2sc\n"
        "INXnWlUwDwYDVR0PAQH/BAUDAwf/gDBlBgNVHSUBAf8EWzBZBggrBgEFBQcDAQYI\n"
        "KwYBBQUHAwIGCCsGAQUFBwMDBggrBgEFBQcDBAYIKwYBBQUHAwgGCCsGAQUFBwMJ\n"
        "BggrBgEFBQcDDQYIKwYBBQUHAw4GBysGAQUCAwUwDQYJKoZIhvcNAQELBQADggEB\n"
        "ABcyab7F7OAsjUSW0YWkVRX1SUMkW25xLLs8koXhHrdnBqgnmOur0xO72/fmTTX9\n"
        "KnCUmQj+dAOmmZrAaIZzqLtMyp4ibHZPfOBwmM0MFnyuwyEnCEYvjPN3FTB0HEgS\n"
        "vCoFH1001LVi4oC1mEMxYaNW4/5Tgl+DTqGF+tctJe3hvbxh+Uu5M0320VAvASjt\n"
        "cJ0me6Ug1FJJ60tgXgZ+M/8V6AXhrQGNgN6WkPMFbbLi5IyEld186QPeLdZ8vCtz\n"
        "StjIV9HZGR1XLotlXarbjVtjxavZJjtwiySeYkAgG7Zjy7LalPSJiIdAD3R/ny+S\n"
        "9kXDKiw/HgYxb8xiy9gdlSc=\n"
        "-----END CERTIFICATE-----\n";

static const char *remote_identity_trusted_expired =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIEKTCCAxGgAwIBAgIBBjANBgkqhkiG9w0BAQsFADByMQswCQYDVQQGEwJOTDEL\n"
        "MAkGA1UECBMCT1YxEzARBgNVBAoTCkFETGluayBJU1QxGTAXBgNVBAMTEElkZW50\n"
        "aXR5IENBIFRlc3QxJjAkBgkqhkiG9w0BCQEWF2luZm9AaXN0LmFkbGlua3RlY2gu\n"
        "Y29tMB4XDTE4MDMwMTAwMDAwMFoXDTE4MDQyMzIzNTk1OVowcjELMAkGA1UEBhMC\n"
        "TkwxCzAJBgNVBAgTAk9WMRMwEQYDVQQKEwpBRExpbmsgSVNUMRkwFwYDVQQDExBC\n"
        "b2IgVGVzdCBFeHBpcmVkMSYwJAYJKoZIhvcNAQkBFhdpbmZvQGlzdC5hZGxpbmt0\n"
        "ZWNoLmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMHm+qG5afCH\n"
        "Fdmp98+Z+POB7vD3WhfghKVugh8FenewuPaaotKxiGAE0kCf8WXU5l4XDR3iBWJs\n"
        "GDHll1yOyL4KW9grwfnfiJMl82uIzKGDiRJbFkXVOLF/DcVLsuqMoRjlk09+nlrg\n"
        "/7xS96itgm6yueW/UOIXpo+ElCHwnrYnpuHHrYM0s7t1ls3QFSyda3INx+LkM6N/\n"
        "b+veVJF95yfe4GvVBhFpLkWtX4xKgZyMjczvmZ6LVNNSnthPhlZFHmspQyqXrTnG\n"
        "yax3qyoloqh9ibDvAK5pUnzqlv/Dyyt110q3bXFR5A0wqfj0fqtp7Mw4TdTkFQy1\n"
        "n4/Y4EuT8I8CAwEAAaOByTCBxjAMBgNVHRMEBTADAQH/MB0GA1UdDgQWBBRCnEs8\n"
        "dPmj8ZBHB0HDaxwg1edaVTAfBgNVHSMEGDAWgBRNVUJNFzhJPReYT4QSx6dK53CX\n"
        "CTAPBgNVHQ8BAf8EBQMDB/+AMGUGA1UdJQEB/wRbMFkGCCsGAQUFBwMBBggrBgEF\n"
        "BQcDAgYIKwYBBQUHAwMGCCsGAQUFBwMEBggrBgEFBQcDCAYIKwYBBQUHAwkGCCsG\n"
        "AQUFBwMNBggrBgEFBQcDDgYHKwYBBQIDBTANBgkqhkiG9w0BAQsFAAOCAQEAdY5n\n"
        "5ElOhpHq/YPWUs68t8HNIhqfokqjLZAgzNyU5QFppb9tPpmFCugerfjlScNwp5HB\n"
        "X6/WjK4runDrgzXfmrBogR4Kscb1KJSm8KAmnzXVUNr1iyASlHxI7241kYdQvTH2\n"
        "LL6b0kjsD5lKAnNh4id0SDHfy/CKg5d7dUxxO1mX48jUiIZtmFqgjej8tFLHy/w/\n"
        "usI5ErlI0qzI6lkoRxPCEWLbXWeBDm3/smHeDbYa/+Lw4Bid8U1+ZSAuC1CT7a7F\n"
        "O3gAjPUL0jzRztp5Yj3dYPV8YyJHLEKr75IXNedV9YKhT4f6kTS3UEjMTqYbYsix\n"
        "MtqgY283RjsExzjNvw==\n"
        "-----END CERTIFICATE-----\n";

static const char *remote_identity_revoked =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDRDCCAiwCFCxXj0QLcpHA597b3QgDf0J3tnQ1MA0GCSqGSIb3DQEBCwUAMF8x\n"
        "CzAJBgNVBAYTAk5MMRMwEQYDVQQIEwpTb21lLVN0YXRlMSEwHwYDVQQKExhJbnRl\n"
        "cm5ldCBXaWRnaXRzIFB0eSBMdGQxGDAWBgNVBAMTD0NIQU01MDAgcm9vdCBjYTAg\n"
        "Fw0yMTA2MDkyMDU2MDFaGA8yMjIxMDQyMjIwNTYwMVowXDELMAkGA1UEBhMCTkwx\n"
        "EzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMg\n"
        "UHR5IEx0ZDEVMBMGA1UEAwwMQ0hBTTUwMSBjZXJ0MIIBIjANBgkqhkiG9w0BAQEF\n"
        "AAOCAQ8AMIIBCgKCAQEAxQJq8im5z1/RZcXS5OKM4PvjH5PXoEIPuIHEZXPH8Eqp\n"
        "jB3Wi9pUIE27nd8Gm3+i4IUBrOUx/LUtmI+k2sd87LxdNsuYm/yVExhAKbYRGW8P\n"
        "90XOAbivMILgjYPitKJazhvKEQLTkMVKO8pcBVtl6KWy15gyLd6eCxXfXPfdJkrO\n"
        "1zYmx4FXttUq7z/gJBRkbV2fb5Tb5lX/8VbjynvYYGGFAexH04XxnHnbrHY/4MoP\n"
        "nTYcZqyEaALrT3Lcv2UrJJTw0mpUCrIy9LReKVIeOGrd9wE0jt3qk41EFZ2RWo8C\n"
        "IL3GfYqo1QtEzAbzsAAXL9S1HUN0OWJV+NoUqqzSvwIDAQABMA0GCSqGSIb3DQEB\n"
        "CwUAA4IBAQC0yJZXJv8nLGG2/60jmG8BobLn6Cas1CLkpVch9N0/e698PHxRqHfs\n"
        "9R/SG6kpfJdOOeBNdw2Z3/s0E2Vuan++DEgAyvVLEHI1RHUue+0GvdyeNJSst/iz\n"
        "1jyFm0nvaiT/jVYpM86c+R0emAtr3rBtxkh/Kop4TM1SOEzutIB4w/vXqklXD5ui\n"
        "XAzosVskfkcnt24c7U9mf9JQt73lB1HbXkyivuQ1lAaVAfhUCSOXi/p3whELeTjL\n"
        "y56eGZ9PcGfDP6NW7YQBKmATkwUwMJzEseM3amwOOd6bHvpeuJzXGLHe92D2e/gr\n"
        "GHRAQsBeyglL3TH2CliOouhtKIoFeVZe\n"
        "-----END CERTIFICATE-----\n";

static const char *crl =
        "data:,-----BEGIN X509 CRL-----\n"
        "MIIB4zCBzAIBATANBgkqhkiG9w0BAQsFADBfMQswCQYDVQQGEwJOTDETMBEGA1UE\n"
        "CBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRk\n"
        "MRgwFgYDVQQDEw9DSEFNNTAwIHJvb3QgY2EXDTIxMDcxNDE0NTIzNFoYDzIyMjEw\n"
        "NTI3MTQ1MjM0WjAnMCUCFCxXj0QLcpHA597b3QgDf0J3tnQ1Fw0yMTA2MDkyMjA1\n"
        "MjdaoA4wDDAKBgNVHRQEAwIBAjANBgkqhkiG9w0BAQsFAAOCAQEAWDkxzaGqznJE\n"
        "bB9PvSGrfRVYFJous5bFqCqB9jJ3nFBh/mQeqdpL9Eiw2rfSWIHbQAqPhDUkGY0t\n"
        "z34aPeIxZZDTDzZZz4bkwt8dLrQwF+rRYEawDpATIoBUeQghVRyhrFNRa4VhtTgc\n"
        "tG55Rt2upphLVqaMMQFHpFlVaMx3P09sXwH2XtPjg+MWYiw/HP6iETPzwBBwGFKb\n"
        "jsS1J7lEJFcdHBCshkloxiZ0B8bj//+mpf0cUTOtn+oVv8adE7REbv1+6B3XV0wQ\n"
        "3Yw16d9jYJb5cFPHK3voGbzuU8JhyOM8cA58Phvv0VJoRvoyMcMmgh9Muk5Uz9EI\n"
        "3qRdLdaghA==\n"
        "-----END X509 CRL-----\n";

static struct plugins_hdl *plugins = NULL;
static dds_security_authentication *auth = NULL;
static DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle remote_identity_handle1 = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle remote_identity_handle2 = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_AuthRequestMessageToken g_local_auth_request_token = DDS_SECURITY_TOKEN_INIT;
static DDS_Security_AuthRequestMessageToken g_remote_auth_request_token = DDS_SECURITY_TOKEN_INIT;
static const DDS_Security_BinaryProperty_t *challenge1 = NULL;
static const DDS_Security_BinaryProperty_t *challenge2 = NULL;
static DDS_Security_OctetSeq serialized_participant_data = DDS_SECURITY_SEQUENCE_INIT;
static DDS_Security_ParticipantBuiltinTopicData *remote_participant_data1 = NULL;
static DDS_Security_ParticipantBuiltinTopicData *remote_participant_data2 = NULL;
static DDS_Security_ParticipantBuiltinTopicData *remote_participant_data3 = NULL;
static DDS_Security_GUID_t candidate_participant_guid;
static DDS_Security_GUID_t remote_participant_guid1;
static DDS_Security_GUID_t remote_participant_guid2;
static bool future_challenge_done = false;


#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
#define AUTH_INCLUDE_EC
#include <openssl/ec.h>
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define AUTH_INCLUDE_DH_ACCESSORS
#endif
#else
#error "version not found"
#endif


static const BIGNUM *
dh_get_public_key(
    DH *dhkey)
{
#ifdef AUTH_INCLUDE_DH_ACCESSORS
    const BIGNUM *pubkey, *privkey;
    DH_get0_key(dhkey, &pubkey, &privkey);
    return pubkey;
#else
    return dhkey->pub_key;
#endif
}


static void
serializer_participant_data(
    DDS_Security_ParticipantBuiltinTopicData *pdata,
    unsigned char **buffer,
    size_t *size);

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

static char *
get_openssl_error(
        void)
{
    BIO *bio = BIO_new(BIO_s_mem());
    char *msg;
    char *buf = NULL;
    size_t len;

    if (bio) {
        ERR_print_errors(bio);
        len = (size_t) BIO_get_mem_data (bio, &buf);
        msg = (char *) ddsrt_malloc(len + 1);
        memset(msg, 0, len+1);
        memcpy(msg, buf, len);
        BIO_free(bio);
    } else {
        msg = ddsrt_strdup("BIO_new failed");
    }

    return msg;
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
initialize_permissions_token(
    DDS_Security_PermissionsToken *token,
    const char *caAlgo)
{
    token->class_id = ddsrt_strdup(DDS_ACTOKEN_PERMISSIONS_CLASS_ID);
    token->properties._length = 2;
    token->properties._maximum = 2;
    token->properties._buffer = DDS_Security_PropertySeq_allocbuf(4);
    token->properties._buffer[0].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CERT_SN);
    token->properties._buffer[0].value = ddsrt_strdup(SUBJECT_NAME_IDENTITY_CA);
    token->properties._buffer[1].name = ddsrt_strdup(DDS_ACTOKEN_PROP_PERM_CA_SN);
    token->properties._buffer[1].value = ddsrt_strdup(caAlgo);
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
deinitialize_identity_token(
    DDS_Security_IdentityToken *token)
{
    DDS_Security_DataHolder_deinit(token);
}

static int
validate_local_identity(const char *trusted_ca_dir, const char *crl_data)
{
    int res = 0;
    DDS_Security_ValidationResult_t result;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GUID_t local_participant_guid;
    DDS_Security_GuidPrefix_t prefix = {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb};
    DDS_Security_EntityId_t entityId = {{0xb0,0xb1,0xb2},0x1};
    DDS_Security_ParticipantBuiltinTopicData *local_participant_data;
    unsigned char *sdata;
    size_t sz;
    unsigned participant_qos_size = 3;
    unsigned offset = 0;
    DDS_Security_Property_t *valbuf;

    trusted_ca_dir ? participant_qos_size++ : participant_qos_size;
    crl_data ? participant_qos_size++ : participant_qos_size;

    memset(&local_participant_guid, 0, sizeof(local_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    memset(&participant_qos, 0, sizeof(participant_qos));
    dds_security_property_init(&participant_qos.property.value, participant_qos_size);

    valbuf = &participant_qos.property.value._buffer[offset++];
    valbuf->name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
    valbuf->value = ddsrt_strdup(identity_certificate);

    valbuf = &participant_qos.property.value._buffer[offset++];
    valbuf->name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
    valbuf->value = ddsrt_strdup(identity_ca);

    valbuf = &participant_qos.property.value._buffer[offset++];
    valbuf->name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
    valbuf->value = ddsrt_strdup(private_key);

    if (trusted_ca_dir != NULL) {
        char trusted_ca_dir_path[1024];
#ifdef WIN32
        snprintf(trusted_ca_dir_path, 1024, "%s\\validate_begin_handshake_reply\\etc\\%s", CONFIG_ENV_TESTS_DIR, trusted_ca_dir);
#else
        snprintf(trusted_ca_dir_path, 1024, "%s/validate_begin_handshake_reply/etc/%s", CONFIG_ENV_TESTS_DIR, trusted_ca_dir);
#endif
        valbuf = &participant_qos.property.value._buffer[offset++];
        valbuf->name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR);
        valbuf->value = ddsrt_strdup(trusted_ca_dir_path);
    }

    if (crl_data != NULL) {
      valbuf = &participant_qos.property.value._buffer[offset++];
      valbuf->name = ddsrt_strdup(ORG_ECLIPSE_CYCLONEDDS_SEC_AUTH_CRL);
      valbuf->value = ddsrt_strdup(crl_data);
    }

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

    local_participant_data = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&local_participant_data->key[0], &local_participant_guid, 12);
    /* convert from big-endian format to native format */
    local_participant_data->key[0] = ddsrt_fromBE4u(local_participant_data->key[0]);
    local_participant_data->key[1] = ddsrt_fromBE4u(local_participant_data->key[1]);
    local_participant_data->key[2] = ddsrt_fromBE4u(local_participant_data->key[2]);

    initialize_identity_token(&local_participant_data->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    initialize_permissions_token(&local_participant_data->permissions_token, RSA_2048_ALGORITHM_NAME);

    local_participant_data->security_info.participant_security_attributes = 0x01;
    local_participant_data->security_info.plugin_participant_security_attributes = 0x02;

    serializer_participant_data(local_participant_data, &sdata, &sz);

    serialized_participant_data._length = serialized_participant_data._maximum = (uint32_t) sz;
    serialized_participant_data._buffer = sdata;

    DDS_Security_ParticipantBuiltinTopicData_free(local_participant_data);

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
        local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    }
    DDS_Security_OctetSeq_deinit(&serialized_participant_data);
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
    size_t sz;

    name = X509_get_subject_name(cert);
    sz = (size_t) i2d_X509_NAME(name, &tmp);
    if (sz > 0) {
        subject = ddsrt_malloc( sz);
        memcpy(subject, tmp, sz);
        OPENSSL_free(tmp);

        SHA256(subject, sz, high);
        SHA256(&candidate->prefix[0], sizeof(DDS_Security_GuidPrefix_t), low);

        adjusted->entityId = candidate->entityId;
        for (i = 0; i < 6; i++) {
            adjusted->prefix[i] = hb | high[i]>>1;
            hb = (unsigned char) ( high[i]<<7 );
        }
        for (i = 0; i < 6; i++) {
            adjusted->prefix[i+6] = low[i];
        }
        ddsrt_free(subject);
        result = 1;
    }

    return result;
}

static int
set_dh_public_key(
    const char *keystr,
    unsigned char **pubkey,
    size_t *size)
{
    int r = 0;
    BIO *bio = NULL;
    EVP_PKEY *pkey;
    DH *dhkey;
    unsigned char *buffer = NULL;
    ASN1_INTEGER *asn1int;

    *pubkey = NULL;


    /* load certificate in buffer */
    bio = BIO_new_mem_buf((void *) keystr, -1);
    if (!bio) {
        char *msg = get_openssl_error();
        r = -1;
        printf("BIO_new_mem_buf failed: %s", msg);
        ddsrt_free(msg);
        goto fail_alloc_bio;
    }

    pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    if (!pkey) {
        char *msg = get_openssl_error();
        r = -1;
        printf("Failed to read public key: %s", msg);
        ddsrt_free(msg);
        goto fail_key_read;
    }

    dhkey = EVP_PKEY_get1_DH(pkey);
    if (!dhkey) {
        char *msg = get_openssl_error();
        r = -1;
        printf("Failed to get DH key from PKEY: %s", msg);
        ddsrt_free(msg);
        goto fail_get_dhkey;
    }

    asn1int = BN_to_ASN1_INTEGER(dh_get_public_key(dhkey), NULL);

    if (!asn1int) {
        char *msg = get_openssl_error();
        r = -1;
        printf("Failed to convert DH key to ASN1 integer: %s", msg);
        ddsrt_free(msg);
        goto fail_get_pubkey;
    }

    *size = (size_t)i2d_ASN1_INTEGER(asn1int, &buffer);

    *pubkey = ddsrt_malloc(*size);
    memcpy(*pubkey, buffer, *size);
    OPENSSL_free(buffer);

    ASN1_INTEGER_free(asn1int);

fail_get_pubkey:
    DH_free(dhkey);
fail_get_dhkey:
    EVP_PKEY_free(pkey);
fail_key_read:
    BIO_free(bio);
fail_alloc_bio:
    return r;
}


static int
set_dh_keys(void)
{
    int r;

    r =  set_dh_public_key(diffie_hellman_pubkey, &dh_pubkey_modp_2048_value, &dh_pubkey_modp_2048_length);
    if (r) {
        invalid_dh_pubkey_modp_2048_value = ddsrt_malloc(dh_pubkey_modp_2048_length);
        memcpy(invalid_dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length);
        invalid_dh_pubkey_modp_2048_value[0] = 0x8;
    }
    return r;
}


static int
validate_remote_identities (const char *remote_id_certificate)
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
                &remote_identity_handle1,
                &g_local_auth_request_token,
                NULL,
                local_identity_handle,
                &remote_identity_token,
                &remote_participant_guid1,
                &exception);

    if ((result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST) &&
        (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)) {
        res = -1;
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
        res = -1;
        printf("validate_remote_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    reset_exception(&exception);

    deinitialize_identity_token(&remote_identity_token);
    DDS_Security_DataHolder_deinit(&local_auth_request_token);

    remote_participant_data1 = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&remote_participant_data1->key[0], &remote_participant_guid1, 12);
    remote_participant_data1->key[0] = ddsrt_fromBE4u(remote_participant_data1->key[0]);
    remote_participant_data1->key[1] = ddsrt_fromBE4u(remote_participant_data1->key[1]);
    remote_participant_data1->key[2] = ddsrt_fromBE4u(remote_participant_data1->key[2]);


    initialize_identity_token(&remote_participant_data1->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    initialize_permissions_token(&remote_participant_data1->permissions_token, RSA_2048_ALGORITHM_NAME);

    remote_participant_data1->security_info.participant_security_attributes = 0x01;
    remote_participant_data1->security_info.plugin_participant_security_attributes = 0x02;

    remote_participant_data2 = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&remote_participant_data2->key[0], &remote_participant_guid2, 12);
    remote_participant_data2->key[0] = ddsrt_fromBE4u(remote_participant_data2->key[0]);
    remote_participant_data2->key[1] = ddsrt_fromBE4u(remote_participant_data2->key[1]);
    remote_participant_data2->key[2] = ddsrt_fromBE4u(remote_participant_data2->key[2]);

    initialize_identity_token(&remote_participant_data2->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    initialize_permissions_token(&remote_participant_data2->permissions_token, RSA_2048_ALGORITHM_NAME);

    remote_participant_data2->security_info.participant_security_attributes = 0x01;
    remote_participant_data2->security_info.plugin_participant_security_attributes = 0x02;

    remote_participant_data3 = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&remote_participant_data3->key[0], &candidate_participant_guid, 12);

    initialize_identity_token(&remote_participant_data3->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    initialize_permissions_token(&remote_participant_data3->permissions_token, RSA_2048_ALGORITHM_NAME);

    remote_participant_data2->security_info.participant_security_attributes = 0x01;
    remote_participant_data2->security_info.plugin_participant_security_attributes = 0x02;

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
        remote_identity_handle1 = DDS_SECURITY_HANDLE_NIL;
    }
    if (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL) {
        success = auth->return_identity_handle(auth, remote_identity_handle2, &exception);
        if (!success) {
            printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
        }
        reset_exception(&exception);
        remote_identity_handle2 = DDS_SECURITY_HANDLE_NIL;
    }

    DDS_Security_DataHolder_deinit(&g_local_auth_request_token);
    DDS_Security_DataHolder_deinit(&g_remote_auth_request_token);

    DDS_Security_ParticipantBuiltinTopicData_free(remote_participant_data1);
    DDS_Security_ParticipantBuiltinTopicData_free(remote_participant_data2);
    DDS_Security_ParticipantBuiltinTopicData_free(remote_participant_data3);
    remote_participant_data1 = NULL;
    remote_participant_data2 = NULL;
    remote_participant_data3 = NULL;
}

static void init_testcase(void)
{

    int res = 0;
    /* Only need the authentication plugin. */
    plugins = load_plugins(NULL   /* Access Control */,
                           &auth  /* Authentication */,
                           NULL   /* Cryptograpy    */,
                           &(const struct ddsi_domaingv){ .handshake_include_optional = true });

    if (plugins) {
        res = validate_local_identity( NULL, NULL );
        if (res == 0) {
            res = validate_remote_identities( remote_identity_certificate );
        }
        if (res == 0){
            res = set_dh_keys();
        }
    } else {
        res = -1;
    }

    CU_ASSERT_FATAL( res == 0 );
}

static void fini_testcase(void)
{
    release_local_identity();
    release_remote_identities();
    unload_plugins(plugins);
    ddsrt_free(invalid_dh_pubkey_modp_2048_value);
    ddsrt_free(dh_pubkey_modp_2048_value);

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
set_binary_property_value(
     DDS_Security_BinaryProperty_t *bp,
     const char *name,
     const unsigned char *data,
     size_t length)
{
    assert(bp);
    assert(name);
    assert(data);

    bp->name = ddsrt_strdup(name);
    bp->value._maximum = bp->value._length = (DDS_Security_unsigned_long) length;
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
    size_t length;

    assert(bp);
    assert(name);
    assert(data);

    length = strlen(data) + 1;
    set_binary_property_value(bp, name, (const unsigned char *)data, length);
}

static void
fill_handshake_message_token(
    DDS_Security_HandshakeMessageToken *token,
    DDS_Security_ParticipantBuiltinTopicData *pdata,
    const char *certificate,
    const char *dsign,
    const char *kagree,
    const unsigned char *diffie_hellman,
    const size_t diffie_hellman_size,
    const unsigned char *challengeData,
    size_t challengeDataSize)
{
    DDS_Security_BinaryProperty_t *tokens;
    DDS_Security_BinaryProperty_t *c_id;
    DDS_Security_BinaryProperty_t *c_perm;
    DDS_Security_BinaryProperty_t *c_pdata;
    DDS_Security_BinaryProperty_t *c_dsign_algo;
    DDS_Security_BinaryProperty_t *c_kagree_algo;
    DDS_Security_BinaryProperty_t *hash_c1;
    DDS_Security_BinaryProperty_t *dh1;
    DDS_Security_BinaryProperty_t *challenge;
    unsigned char *serialized_local_participant_data;
    size_t serialized_local_participant_data_size;
    unsigned char hash[32];

    serializer_participant_data(pdata, &serialized_local_participant_data, &serialized_local_participant_data_size);

    tokens = DDS_Security_BinaryPropertySeq_allocbuf(8);
    c_id = &tokens[0];
    c_perm = &tokens[1];
    c_pdata = &tokens[2];
    c_dsign_algo = &tokens[3];
    c_kagree_algo = &tokens[4];
    hash_c1 = &tokens[5];
    dh1 = &tokens[6];
    challenge = &tokens[7];

    /* Store the Identity Certificate associated with the local identify in c.id property */
    if (certificate) {
        set_binary_property_string(c_id, DDS_AUTHTOKEN_PROP_C_ID, certificate);
    } else {
        set_binary_property_string(c_id, DDS_AUTHTOKEN_PROP_C_ID "x", "rubbish");
    }

    /* Store the permission document in the c.perm property */
    set_binary_property_string(c_perm, DDS_AUTHTOKEN_PROP_C_PERM, "permissions_document");

    /* Store the provided local_participant_data in the c.pdata property */
    set_binary_property_value(c_pdata, DDS_AUTHTOKEN_PROP_C_PDATA, serialized_local_participant_data, serialized_local_participant_data_size);

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
        unsigned char *buffer;
        size_t size;

        bseq._length = bseq._maximum = 5;
        bseq._buffer = tokens;

        serializer = DDS_Security_Serializer_new(1024, 1024);

        DDS_Security_Serialize_BinaryPropertySeq(serializer, &bseq);
        DDS_Security_Serializer_buffer(serializer, &buffer, &size);
        SHA256(buffer, size, (unsigned char *)&hash);
        ddsrt_free(buffer);
        DDS_Security_Serializer_free(serializer);

        set_binary_property_value(hash_c1, DDS_AUTHTOKEN_PROP_HASH_C1, (const unsigned char *) &hash, sizeof(hash));
    }

    /* Set the DH public key associated with the local participant in dh1 property */
    if (diffie_hellman) {
        set_binary_property_value(dh1, DDS_AUTHTOKEN_PROP_DH1, diffie_hellman, diffie_hellman_size);
    } else {
        set_binary_property_string(dh1, DDS_AUTHTOKEN_PROP_DH1 "x", "rubbish");
    }

    /* Set the challenge in challenge1 property */
    if (challengeData) {
        set_binary_property_value(challenge, DDS_AUTHTOKEN_PROP_CHALLENGE1, challengeData, challengeDataSize);
    } else {
        set_binary_property_value(challenge, DDS_AUTHTOKEN_PROP_CHALLENGE1 "x", challenge2->value._buffer, challenge2->value._length);
    }

    token->class_id = ddsrt_strdup(AUTH_HANDSHAKE_REQUEST_TOKEN_CLASS_ID);
    token->binary_properties._length = token->binary_properties._maximum = 8;
    token->binary_properties._buffer = tokens;

    ddsrt_free(serialized_local_participant_data);
}

static void
fill_handshake_message_token_default(
    DDS_Security_HandshakeMessageToken *token,
    DDS_Security_ParticipantBuiltinTopicData *pdata,
    const unsigned char *challengeData,
    unsigned int challengeDataSize)
{
    fill_handshake_message_token(
            token, pdata, remote_identity_certificate,
            AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
            dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challengeData, challengeDataSize);
}

static void
handshake_message_deinit(
    DDS_Security_HandshakeMessageToken *token)
{
    DDS_Security_DataHolder_deinit(token);
}

static bool
validate_handshake_token(
    DDS_Security_HandshakeMessageToken *token,
    const DDS_Security_OctetSeq *challenge_1,
    const DDS_Security_OctetSeq *challenge_2)
{
    const DDS_Security_BinaryProperty_t *property;

    if (!token->class_id || strcmp(token->class_id, AUTH_HANDSHAKE_REPLY_TOKEN_CLASS_ID) != 0) {
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
    } else if (!valid_string_value(AUTH_KAGREE_ALGO_RSA_NAME, &property->value)) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO"' incorrect value");
    } else if (find_binary_property(token, DDS_AUTHTOKEN_PROP_HASH_C2) == NULL) {
         CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_HASH_C2"' not found");
    } else if (find_binary_property(token, DDS_AUTHTOKEN_PROP_DH2) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_DH2"' not found");
    } else if (find_binary_property(token, DDS_AUTHTOKEN_PROP_HASH_C1) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_HASH_C1"' not found");
    } else if (find_binary_property(token, DDS_AUTHTOKEN_PROP_DH1) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_DH1"' not found");
    } else if ((property = find_binary_property(token, DDS_AUTHTOKEN_PROP_CHALLENGE1)) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_CHALLENGE1"' not found");
    } else if (challenge_1 && compare_octet_seq(challenge_1, &property->value) != 0) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_CHALLENGE1"' incorrect value");
    } else if ((property = find_binary_property(token, DDS_AUTHTOKEN_PROP_CHALLENGE2)) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_CHALLENGE2"' not found");
    } else if (challenge_2 && compare_octet_seq(challenge_2, &property->value) != 0) {
        CU_FAIL("HandshakeMessageToken incorrect property '"DDS_AUTHTOKEN_PROP_CHALLENGE2"' incorrect value");
    } else {
        return true;
    }

    return false;
}

CU_Test(ddssec_builtin_validate_begin_handshake_reply, happy_day,  .init = init_testcase, .fini = fini_testcase)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token_default(&handshake_token_in, remote_participant_data1, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT(handshake_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT(validate_handshake_token(&handshake_token_out, &challenge2->value, NULL));

    reset_exception(&exception);

    success= auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
}

CU_Test(ddssec_builtin_validate_begin_handshake_reply,future_challenge,  .init = init_testcase, .fini = fini_testcase)

{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != NULL);

    fill_handshake_message_token_default(&handshake_token_in, remote_participant_data2, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle1,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT(handshake_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT(validate_handshake_token(&handshake_token_out, &challenge2->value, &challenge1->value));

    reset_exception(&exception);

    success= auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);

    future_challenge_done = true;
}


CU_Test(ddssec_builtin_validate_begin_handshake_reply,invalid_arguments,  .init = init_testcase, .fini = fini_testcase)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_OctetSeq serdata = DDS_SECURITY_SEQUENCE_INIT;

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token_default(&handshake_token_in, remote_participant_data1, challenge1->value._buffer, challenge1->value._length);

    result = auth->begin_handshake_reply(
                       auth,
                       NULL,
                       &handshake_token_out,
                       &handshake_token_in,
                       remote_identity_handle1,
                       local_identity_handle,
                       &serialized_participant_data,
                       &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->begin_handshake_reply(
                     auth,
                     &handshake_handle,
                     NULL,
                     &handshake_token_in,
                     remote_identity_handle1,
                     local_identity_handle,
                     &serialized_participant_data,
                     &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->begin_handshake_reply(
                     auth,
                     &handshake_handle,
                     &handshake_token_out,
                     NULL,
                     remote_identity_handle1,
                     local_identity_handle,
                     &serialized_participant_data,
                     &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->begin_handshake_reply(
                     auth,
                     &handshake_handle,
                     &handshake_token_out,
                     &handshake_token_in,
                     0,
                     local_identity_handle,
                     &serialized_participant_data,
                     &exception);

     if (result != DDS_SECURITY_VALIDATION_OK) {
         printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
     }
     CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);
     reset_exception(&exception);

     result = auth->begin_handshake_reply(
                      auth,
                      &handshake_handle,
                      &handshake_token_out,
                      &handshake_token_in,
                      remote_identity_handle1,
                      0,
                      &serialized_participant_data,
                      &exception);

     if (result != DDS_SECURITY_VALIDATION_OK) {
         printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
     }
     CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);
     reset_exception(&exception);

     result = auth->begin_handshake_reply(
                     auth,
                     &handshake_handle,
                     &handshake_token_out,
                     &handshake_token_in,
                     remote_identity_handle1,
                     local_identity_handle,
                     NULL,
                     &exception);

     if (result != DDS_SECURITY_VALIDATION_OK) {
         printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
     }
     CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);
     reset_exception(&exception);

     result = auth->begin_handshake_reply(
                     auth,
                     &handshake_handle,
                     &handshake_token_out,
                     &handshake_token_in,
                     remote_identity_handle1,
                     local_identity_handle,
                     &serdata,
                     &exception);

     if (result != DDS_SECURITY_VALIDATION_OK) {
         printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
     }
     CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);
     reset_exception(&exception);

     handshake_message_deinit(&handshake_token_in);
}


CU_Test(ddssec_builtin_validate_begin_handshake_reply,invalid_certificate,  .init = init_testcase, .fini = fini_testcase)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token(
            &handshake_token_in, remote_participant_data1, unrelated_identity,
            AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
            dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);
    handshake_message_deinit(&handshake_token_in);

    fill_handshake_message_token(
             &handshake_token_in, remote_participant_data1, NULL,
             AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
             dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
}

CU_Test(ddssec_builtin_validate_begin_handshake_reply,invalid_participant_data ,  .init = init_testcase, .fini = fini_testcase)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_BinaryProperty_t *property;

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token(
            &handshake_token_in, remote_participant_data3, remote_identity_certificate,
            AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
            dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);

    fill_handshake_message_token(
            &handshake_token_in, remote_participant_data1, remote_identity_certificate,
            AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
            dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    property = find_binary_property(&handshake_token_in, "c.pdata");
    CU_ASSERT_FATAL(property != NULL);
    assert(property != NULL); // for Clang's static analyzer

    ddsrt_free(property->name);
    property->name = ddsrt_strdup("c.pdatax");

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
}


CU_Test(ddssec_builtin_validate_begin_handshake_reply,invalid_dsign_algo ,  .init = init_testcase, .fini = fini_testcase)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token(
            &handshake_token_in, remote_participant_data1, remote_identity_certificate,
            "RSASSA-PSS-SHA128", AUTH_KAGREE_ALGO_RSA_NAME,
            dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);

    fill_handshake_message_token(
             &handshake_token_in, remote_participant_data1, remote_identity_certificate,
             NULL, AUTH_KAGREE_ALGO_RSA_NAME,
             dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

     result = auth->begin_handshake_reply(
                     auth,
                     &handshake_handle,
                     &handshake_token_out,
                     &handshake_token_in,
                     remote_identity_handle2,
                     local_identity_handle,
                     &serialized_participant_data,
                     &exception);

     if (result == DDS_SECURITY_VALIDATION_OK) {
         printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
     }

     CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);

     reset_exception(&exception);

     handshake_message_deinit(&handshake_token_in);
}

CU_Test(ddssec_builtin_validate_begin_handshake_reply,invalid_kagree_algo ,  .init = init_testcase, .fini = fini_testcase)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token(
            &handshake_token_in, remote_participant_data1, remote_identity_certificate,
            AUTH_DSIGN_ALGO_RSA_NAME, "DH+MODP-2048-128",
            dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);

    fill_handshake_message_token(
             &handshake_token_in, remote_participant_data1, remote_identity_certificate,
             AUTH_DSIGN_ALGO_RSA_NAME, NULL,
             dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

     result = auth->begin_handshake_reply(
                     auth,
                     &handshake_handle,
                     &handshake_token_out,
                     &handshake_token_in,
                     remote_identity_handle2,
                     local_identity_handle,
                     &serialized_participant_data,
                     &exception);

     if (result == DDS_SECURITY_VALIDATION_OK) {
         printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
     }

     CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);

     reset_exception(&exception);

     handshake_message_deinit(&handshake_token_in);
}

CU_Test(ddssec_builtin_validate_begin_handshake_reply,invalid_diffie_hellman ,  .init = init_testcase, .fini = fini_testcase)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token(
            &handshake_token_in, remote_participant_data1, remote_identity_certificate,
            AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
            invalid_dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);

    fill_handshake_message_token(
             &handshake_token_in, remote_participant_data1, remote_identity_certificate,
             AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
             NULL, 0, challenge2->value._buffer, challenge2->value._length);

     result = auth->begin_handshake_reply(
                     auth,
                     &handshake_handle,
                     &handshake_token_out,
                     &handshake_token_in,
                     remote_identity_handle2,
                     local_identity_handle,
                     &serialized_participant_data,
                     &exception);

     if (result == DDS_SECURITY_VALIDATION_OK) {
         printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
     }

     CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);

     reset_exception(&exception);

     handshake_message_deinit(&handshake_token_in);
}

CU_Test(ddssec_builtin_validate_begin_handshake_reply,invalid_challenge ,  .init = init_testcase, .fini = fini_testcase)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token_default(&handshake_token_in, remote_participant_data2, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
      auth,
      &handshake_handle,
      &handshake_token_out,
      &handshake_token_in,
      remote_identity_handle1,
      local_identity_handle,
      &serialized_participant_data,
      &exception);
    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);


    fill_handshake_message_token(
            &handshake_token_in, remote_participant_data1, remote_identity_certificate,
            AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
            dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge1->value._buffer, challenge1->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle1,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);

    fill_handshake_message_token(
             &handshake_token_in, remote_participant_data1, remote_identity_certificate,
             AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
             dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, NULL, 0);

     result = auth->begin_handshake_reply(
                     auth,
                     &handshake_handle,
                     &handshake_token_out,
                     &handshake_token_in,
                     remote_identity_handle1,
                     local_identity_handle,
                     &serialized_participant_data,
                     &exception);

     if (result == DDS_SECURITY_VALIDATION_OK) {
         printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
     }

     CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
     CU_ASSERT (exception.minor_code != 0);
     CU_ASSERT (exception.message != NULL);


     reset_exception(&exception);

     handshake_message_deinit(&handshake_token_in);
}


CU_Test(ddssec_builtin_validate_begin_handshake_reply,return_handle,  .init = init_testcase, .fini = fini_testcase)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token_default(&handshake_token_in, remote_participant_data1, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle1,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT (handshake_handle != DDS_SECURITY_HANDLE_NIL);

    reset_exception(&exception);

    success = auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    success = auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT ( success == false );
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);

}

CU_Test(validate_begin_handshake_reply,extended_certificate_check,  .init = init_testcase, .fini = fini_testcase )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    release_local_identity();
    release_remote_identities();

    CU_ASSERT_FATAL( !validate_local_identity("trusted_ca_dir", NULL) );
    CU_ASSERT_FATAL( !validate_remote_identities( remote_identity_trusted ) );

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token(
                    &handshake_token_in, remote_participant_data1, remote_identity_trusted,
                AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
                dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    CU_ASSERT(handshake_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT(validate_handshake_token(&handshake_token_out, &challenge2->value, NULL));

    reset_exception(&exception);

    success= auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);

    release_local_identity();
    release_remote_identities();

    CU_ASSERT_FATAL( !validate_local_identity("trusted_ca_dir", NULL) );
    CU_ASSERT_FATAL( !validate_remote_identities( remote_identity_untrusted ) );

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token(
                    &handshake_token_in, remote_participant_data1, remote_identity_untrusted,
                AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
                dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT_FATAL( exception.code != 0 );

    reset_exception(&exception);

    auth->return_handshake_handle(auth, handshake_handle, &exception);
    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    reset_exception(&exception);


    release_local_identity();
    release_remote_identities();

    CU_ASSERT_FATAL( !validate_local_identity("trusted_ca_dir", NULL) );
    CU_ASSERT_FATAL( !validate_remote_identities( remote_identity_trusted_expired ) );

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token(
                    &handshake_token_in, remote_participant_data1, remote_identity_trusted_expired,
                AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
                dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT_FATAL( exception.code != 0 );

    reset_exception(&exception);


    auth->return_handshake_handle(auth, handshake_handle, &exception);
    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    reset_exception(&exception);
}

CU_Test(validate_begin_handshake_reply,crl,  .init = init_testcase, .fini = fini_testcase )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    release_local_identity();
    release_remote_identities();

    CU_ASSERT_FATAL( !validate_local_identity(NULL, crl) );
    CU_ASSERT_FATAL( !validate_remote_identities( remote_identity_revoked ) );

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert (auth->begin_handshake_reply != 0);

    fill_handshake_message_token(
                    &handshake_token_in, remote_participant_data1, remote_identity_revoked,
                AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
                dh_pubkey_modp_2048_value, dh_pubkey_modp_2048_length, challenge2->value._buffer, challenge2->value._length);

    result = auth->begin_handshake_reply(
                    auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    remote_identity_handle2,
                    local_identity_handle,
                    &serialized_participant_data,
                    &exception);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        printf("begin_handshake_reply failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
}
