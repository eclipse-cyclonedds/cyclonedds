/** @file qos_utests.c
 *  @brief Unit tests for qos APIs
 *
 */
/* CUnit includes. */
#include <stdio.h>
#include <string.h>
#include "dds/ddsrt/environ.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "assert.h"


/* Test helper includes. */
#include "common/src/loader.h"


#include "dds/security/dds_security_api.h"
#include "dds/security/openssl_support.h"

#include <dds/ddsrt/heap.h>
#include <dds/ddsrt/string.h>
#include <config_env.h>

// See ../etc/README.md for how the certificates and keys are generated.

static const char *identity_certificate_filename = "identity_certificate";
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

static const char *identity_ca_filename = "identity_ca";
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

static const char *private_key_filename = "private_key";
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

static const char *unrelated_identity_ca_filename = "unrelated_identity_ca";
static const char *unrelated_identity_ca =
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

static const char *invalid_identity_certificate_filename = "invalid_identity_certificate";
static const char *invalid_identity_certificate =
        "data:,-----BEGIN CERTIFICATE-----\n"
        "MIIDNzCCAh8CCQDn8i4K9c4ErDANBgkqhkiG9w0BAQsFADBfMQswCQYDVQQGEwJO\n"
        "TDETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lkZ2l0\n"
        "cyBQdHkgTHRkMRgwFgYDVTQDDA9DSEFNNTAwIHJvb3QgY2EwHhcNMTgwMjEyMTUw\n"
        "NjUxWhcNMTkwNjI3MTUwNjUxWjBcMQswCQYDVQQGEwJOTDETMBEGA1UECAwKU29t\n"
        "ZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMRUwEwYD\n"
        "VQQDDAxDSEFNNTAwIGNlcnQwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIB\n"
        "AQDRnnNnV3PZrbZwjxk+dkQcO3pK3wMvoDNAHPPiTfXRV2KjLHxsuK7wV+GPHmXo\n"
        "97bot6vBxNQN7hfxoDLL+KBO9s3V+8OX6lOrF7hQ6+6/p9EgRoyNGo21eIzGwc2M\n"
        "aJAKjImNMbM7FDTvhk3u+VTTJtlnKvJM1tgncbEZwRLri/2MEC5XS/O5FQT4AXPr\n"
        "A6bRcGMqCVYtQ0ci6wd18PegA/rSmGSRf/TOd4jZXkxfHD+YOkHcxxz9sX4KnyOg\n"
        "XZm8jDdBc7rxiDep8kIjL06VszJeoQrxjuf8cNZtbol/7ECS5aM2YOx7t0Dc/629\n"
        "V2Q5waRVBV5xVCJ0BzUh8rIFAgMBAAEwDQYJKoZIhvcNAQELBQADggEBAGlkxYLr\n"
        "ZI/XNjDC6RFfSFRoDc+Xpcg+GsJKKbw2+btZvAD8z7ofL01yGru9oi6u2Yy/ZDKT\n"
        "liZ+gtsD8uVyRkS2skq7BvPzvoYErLmSqwlrcCbeX8uHiN7C76ll9PFtSjnwPD//\n"
        "UaNyZM5dJB2eBh4/prclix+RR/FWQzkPqEVLwMcFBmnPZ0mvR2tncjpZq476Qyl9\n"
        "3jcmfms9qBfBPPjCdXqGEDgsTd2PpYRD2WDj/Ctl4rV7B2jnByullLUYIWGu0rYt\n"
        "988waU5i8ie4t/TorBBLqQo/NO9jSXfEqcAnILPnv1QZanKzAAxSg7+FgFrsn359\n"
        "ihiEkx9zFUnPrdA=\n"
        "-----END CERTIFICATE-----\n";

static const char *invalid_identity_ca_filename = "invalid_identity_ca";
static const char *invalid_identity_ca =
        "data:,-----BEGIN CERTIFICATE-----\n"
        "MIIDljCCAn6gAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwXzELMAkGA1UEBhMCTkwx\n"
        "EzARBgNVBAgMCk92ZXJpanNzZWwxEDAOBgNVBAcMB0hlbmdlbG8xDzANBgNVBAoM\n"
        "BkFETElOSzEYMBYGA1UEAwwPQ0hBTTUwMCBSb290IENBMB4XDTE4MDIwOTE2Mjky\n"
        "MVoXDTI4MDIwNzE2MjkyMVowVTELMAkGA1UEBhMCTkwxEzARBgNVBAgMCk92ZXJp\n"
        "anNzZWwxDzANBgNVBAoMBkFETElOSzEgMB4GA1UEAwwXQ0hBTTUwMCBJbnRlcm1l\n"
        "ZGlhdGUgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCwucuU/L6N\n"
        "iYxYJ7tyzcdzwXwYbr4GynZE4u2Sd7hcXrQGxTotm9BEhOZWscSGvH+UJSp0Vrb4\n"
        "3zDppiJ76ys6PeSBw1PpxdO97fO+eAE5DoXRj0a9lmnjbsV6waZ2GxgYQNVmKqbI\n"
        "uPDfW+jsmRcTO94s05GWQshHeiqxuEUAv3/Qe2vOhulrg4YDcXrIDWK93cr1EmRX\n"
        "Eq3Ck+Fjwtk5wAk3TANv2XQkVfS80jYAurL8J+XC2kyYB7e8KO92zqlfVXXMC3NI\n"
        "YDcq86bAI4NNMjVE2zIVheMLoOEXaV7KUTYfEQABZl76aWLDxjED9kf371tcrZzJ\n"
        "6xZ1M/rPGNblAgMBAAGjZjBkMB0GA1UdDgQWBBQngrlZqhQptCR4p04zqHamYUx7\n"
        "RTAfBgNVHSMEGDAWgBQrLYI+RHJtx1Pze8MSZbhaOful1DASBgNVHRMBAf8ECDAG\n"
        "AQH/AgEAMA4GA1UdDwEB/wQEAwIBhjANBgkqhkiG9w0BAQsFAAOCAQEAfMmiQ0tv\n"
        "o3K3xwSS621tsfkijUTx920hAe1XYY2XKrG7a/MJBhStex5A3AfqPOY9UMihkBl9\n"
        "3hgxOaddX9SAf2eLk2JLhqxZi1U/GVzT5h10AKLA5WUXIK4UGz3JRqhEm7V39t/N\n"
        "G0LCdpWOZueezkfO6eGcAvOKthdd32a3zbn+rzzDHdsjzxhEEv8d8x1Xf4xH2dgk\n"
        "HlpmpvXMfG/1aCzIpWGEPdkB7WR694GiCmh7hnFBiY+h1GFj2l5dThd51QqAlncM\n"
        "u+NmlPCrFZL0ulwRFeo80KOwDpxkqgavDlP9irdWqM9VHybjGu0xFHCeElz9M6od\n"
        "ym/MCh4ax7jDxg==\n"
        "-----END CERTIFICATE-----\n";

static const char *identity_certificate_1024key =
        "-----BEGIN CERTIFICATE-----\n"
        "MIICrjCCAZYCCQDn8i4K9c4ErjANBgkqhkiG9w0BAQsFADBfMQswCQYDVQQGEwJO\n"
        "TDETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lkZ2l0\n"
        "cyBQdHkgTHRkMRgwFgYDVQQDDA9DSEFNNTAwIHJvb3QgY2EwHhcNMTgwMjE2MTAy\n"
        "MzM2WhcNMjMwODA5MTAyMzM2WjBXMQswCQYDVQQGEwJOTDETMBEGA1UECAwKU29t\n"
        "ZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMRAwDgYD\n"
        "VQQDDAdDSEFNNTY5MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDS5w0h8L70\n"
        "hkreKchVbTzfz4CrBLY4iADNSqPx9uW7DxjeHyLbKT2eRViY/xPuPXQmfRim01QM\n"
        "sZWKvFr6k9WMsJ6ItNtCyKS/beONqvXOddIu+4IhNzEGs5v4pTJAOzraoZcVmXnf\n"
        "Mr9G/baMYfMG47JR5HaSHDI5esa2STHt4wIDAQABMA0GCSqGSIb3DQEBCwUAA4IB\n"
        "AQBdZ2ijHYH8TkOGBqzsNwnNwPaDb/NA0vAO9T5kSOm8HA8vKHnNza+DeUJN+5P/\n"
        "P4fLK7UZqpQN32MpvXL0068g99RLjAzAsEVn+0FTyc08r9p/KO/dxxdMKeET7Cpv\n"
        "rMpu3W0A/EJptCQsTEZI0iqts7T2qQVXzoDlnUwEt3xdmKYJ9jbEq1UUCeexD3nP\n"
        "LB+JtUtfGevVzIoBjHv0qA3ePA24jDUlx5bxFeoIDC4tEewvUG5ZekftsRdNe3fk\n"
        "3LkwyK+4NN1ZCa2+S5SOAfjZA2o6qXiq/le0vWRgl7AHEgDr6w7xoRsw4K5dQ+0R\n"
        "eKtsBC4XO1GqrNYdKuJb1MhI\n"
        "-----END CERTIFICATE-----\n";

static const char *invalid_private_key_filename = "invalid_private_key";
static const char *invalid_private_key =
        "data:,-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIEowIBAAKCAQEA0Z5zZ1dz2a22cI8ZPnZEHDt6St8DL6AzQBzz4k310Vdioyx8\n"
        "bLiu8Ffhjx5l6Pe26LerwcTUDe4X8aAyy/igTvbN1fvDl+pTqxe4UOvuv6fRIEaM\n"
        "jRqNtXiMxsHNjGiQCoyJjTGzOxQ074ZN7vlU0ybZZyryTNbYJ3GxGcES64v9jBAu\n"
        "V0vzuRUE+AFz6wOm0XBjKglWLUNHIusHdfD3oAP60phkkX/0zneI2V5MXxw/mDpB\n"
        "3Mcc/bF+Cp8joF2ZvIw3QXO68Yg3qfJCIy9OlbMyXqEK8Y7n/HDWbW6Jf+xAkuWj\n"
        "NmDse7dA3P+tvVdkOcGkVQVecVQidAc1IfKyBQIDAQABAoIBAEddUpzUQTTS11Hq\n"
        "5gVF7lpORYxH8KW+PLSPJtjdAduLjKAQ++tn1OcuhDRdfQSbkUIZhfgqMqerb6tr\n"
        "ht+6fZlknR9E34pQ1LtjD/U83cOSNrhuTFudtrEZoZPpVzl+P8vXnNzdFs/+SSdi\n"
        "6hV5/U8F4u4kyOkwG9cR9eF2wiI+oQ/RBKCXUo3OVs9K27A/OkKsb7coL7yBsgBj\n"
        "lzorS9a/DyHT2eiMKjwCZFyG4A66EkLi6t9JLJ8oTkI2WskXYeVEAbEXE57RWm44\n"
        "2OgTgfsgYgf2ftXq93KD17FN1m77dqp7EPAhjGnRHNq7+0Ykr1EO1nbDfqHG4gS+4o\n"
        "lfP8iwECgYEA58da0R34l93yQnK0mAtoobwvsOjADnBVhBg9L4s2eDs8liUjf0zt\n"
        "7hcMdUJaa7iMuNf3qGtnZtRURc3kSOE429Or7fCAYUr/AaA7+2ekPG1vjMb50tVv\n"
        "se5rwb1hvgMYe2L5ktJJAg+RcmqpY+ncJ+hP/vWwZRxUKvXba50qqEkCgYEA54ZE\n"
        "mJfSueGM/63xlhP71CM4OWtTqkQGp2OmgTOsBI5q/GUXr8vMR8sCEMHAc6HyXzmL\n"
        "x/RnAoa/vTX58rXBk0QjfO9esIHa452697EIaJu5w8skCLDv2e/f+Jg7o/IDyUZs\n"
        "5lqhiEuH9Qc3sx2nhnSYXMZWqwh8OchI7dCSE90CgYEAzrJ1JhpxUJYI7wM2VIWQ\n"
        "GPQnH8BhTj8VtEidgCHJQK2rGUcjgepMIVECtiunUXtyW4GWBedKfmSKhvnXRLs9\n"
        "pqT9JaOeCaYFBiEsfMZvqUY4e/YSYtge1PIHvO40FWzTT23zneDUZPcXQY8nYsfy\n"
        "otBFTt0yIumBkhJRTIYLvakCgYA+CcttvBj6OAcJJ/n5RgeP05QoRqsXj7zcs6YV\n"
        "LtxkKClg0lHjiE+H2U0HYnOISJfijk/3V3UWxzavo7wDHlLtfC+qNZYA4/rcTRKh\n"
        "dm2TYk8HuPJB5e+PTWiNe3VXu+zpzRY3L4fjNqIKtVFmjIasT6fYDEmC8PYgoZtx\n"
        "JhdOfQKBgCD/bDkc+VI6lwQtoQQKiSfQjKGe+6Cw9K/obzWO0uJwBvZrGLXF8tTc\n"
        "MOPIv9OILt7DYxpMXAiHv8HtzH5CFVrZ/nj63Soka/j2yvUdBDrGhyIbsc4pDu+\n"
        "lCFa0ZiT/u5vRAiOkM6GuStH4HxnW9LtwBtiYXtfU7IPExJiAlsq\n"
        "-----END RSA PRIVATE KEY-----\n";

static const char *private_key_1024 =
        "data:,-----BEGIN RSA PRIVATE KEY-----\n"
        "MIICXAIBAAKBgQDS5w0h8L70hkreKchVbTzfz4CrBLY4iADNSqPx9uW7DxjeHyLb\n"
        "KT2eRViY/xPuPXQmfRim01QMsZWKvFr6k9WMsJ6ItNtCyKS/beONqvXOddIu+4Ih\n"
        "NzEGs5v4pTJAOzraoZcVmXnfMr9G/baMYfMG47JR5HaSHDI5esa2STHt4wIDAQAB\n"
        "AoGAQi7LijkYU3fJCsql2Vj8X2eogwJphHf5eHLR296U3QyxyxKOR6Q7d+1fDjQN\n"
        "txeF2YYsND3hBFK+ENlm23eE7Z1tpWtnLNJ9OH84ZkPnqTnEcWsRddT/x9vKOPMz\n"
        "eK8QNetD3AP5qXsjpIpgep1diWYHCyhMTAFISzvdtC7pvFECQQD2zoqwRtF26lzg\n"
        "QPR02Z+L80R2VhpeLoqMIplT1bmlrPlDr/eIwvtu1eQFyGSASG2EFs1rucdJl7qu\n"
        "SrJ+eyv1AkEA2sIjy8+RCk1uH8kEwYaMJ3dccqnpcMCZ1b3GncKl+ICmDCYcpfd5\n"
        "rP5tX+GL3RVw370pUApJvrVTgOpAVHYjdwJAOYz8BhLdcS9BLQG4fy7n50h4pGd7\n"
        "io6ru/Wtb0EdIybskP4NaJSe8L9rhnWuCcPZ1b1DdWVCtURuQYoliRzLqQJBAJWO\n"
        "ZrSfKpS1jRVT89lu6ADPXLfTrBH2yvVS8ifG/HshUOQ7ZhidUWVQ6GvFoj46u1lr\n"
        "VIQxFGu6QeV/wQ09W08CQHGkrZgu/FpS2tNvYmKNDHOna+dW452N5N81u5sRP1A8\n"
        "x9pYC9xoOGE2E8v1ocMJDPoMe0yk1QSX9mjhhwYOy28=\n"
        "-----END RSA PRIVATE KEY-----\n";

static const char *private_key_w_password_filename = "private_key_w_password";
static const char *private_key_w_password =
        "data:,-----BEGIN RSA PRIVATE KEY-----\n"
        "Proc-Type: 4,ENCRYPTED\n"
        "DEK-Info: AES-256-CBC,0C9C38C5678AECD200E024A9A5BC717A\n"
        "\n"
        "wbXYsR78o4DIaQbKsB4cwEFge79GMKbcMgIWK+9k86dPk09WZkj7JCSJXIxPLYOG\n"
        "tFZrw/z86cakEhf90a4Moa1cwByrcFB+bpWoEqsx/C4javWxXMENbmQ5x8gDpmzT\n"
        "qqLI7xnd2mYj7HcfE7eXi+Nub5w1tBxN0CWaxpR54ZVvfcPE6Od4SHGughdUN4AK\n"
        "OdVIq5YuzMhuTDJKy+kGOtjH8pBWvo8S12T2UQEuusx5WUbEJ6m+E80aN7M6gCA+\n"
        "XmNTt3PsV9PfZPL2Off/90gqTBdMhwn+sEVlqYG0TAnXZQEI8ZNtAGy77CFplqdT\n"
        "SmI3x8Sza7lchEMFhRiayX9pMBPUlwckVPrCoMQ4b4WkHYoGLO7dNVAA8Osud46+\n"
        "6MZKHStjdzwKz9MzWa7lXhXV+0sX5bcAzQexuE+wO8QQ/t5uwDmQHol0JVdRX8NB\n"
        "/Exk6aT7mWajFvukXVirpUGWnEK2W+O8/VBzVZ7z69EjZ09Pu4Y/+cbX12LGSwMb\n"
        "WVtnZY6BsrV++vikQuL3ByTBDPRHio2H8hThh1Kv8n5VEBUrk7tLUp3Z15cbfC6s\n"
        "LDHX6kB2OKmmdqLOOMo4lToZfnrVK/dzeXFbtNH1POpR4/e5Nk0SyZrWo+E/AqIv\n"
        "nLQ3fhLCPOB5rjAhuM8iXOwqn8HHNlv9j8mlgCcgwK7focYVc/IXARLOfFOjOA/s\n"
        "EqMRbb/eKsC930NHBZkhlqRJKCwA37AMvnOhN4R0VOq/40K+62IUK9E1643KyWs0\n"
        "vWk0rFY0OKorQXzI33lbBYZ5zHt8oxGNx3us+6jGP85iv8UEaO6FpgajEUn6Gzp/\n"
        "wyvr1C/B/Hfr6eTbt4C6Fi5fMfZgM6VEcJuaFZnoC9tWdhlNsY1pwtPghMM7yBwc\n"
        "1Ye0TxdF36exJWu1gXVTse1Vdc9i4QWpT2fbPQtcIgdtNk01K+2defie384IOnQQ\n"
        "O8/SRsrnLRLV3IDFh/VBJS1ZVm8Zmt10yGgRwtYHntMkIopoFRWcm9/Gh3iBFKKH\n"
        "OTVXxgKOUYk4qXG61N8k0M+TIdoOHZIha3Myis1tQVmA/b/4FRKPYgdrFijhXNLM\n"
        "wwMHQOS14xBF2KBgaak7dMUWhGrClw1hc3HmMXuM+OLvxy+f8MC3JP2U5AuCs472\n"
        "hc41KWxioqNMPVXZgVnHf3aEec+hBFceqYnlzG+E/Gagiufu8WMySaZgzXMRb5aV\n"
        "x2OVcakSKrTC5EKbLDlZ6+1NRJht7wdSefh0o/Crc9LzcFqBL0Qp3WCvyY8gDVkQ\n"
        "EGqoVYOE6Hz/NX5/17F1+5VuWT7nBWABHKFFriOoJ3VR9sZhp0HiMznZEF1AwVuZ\n"
        "xHtDWQemfBywEQG23qbr+o7mQASh1zki8b4fP1HQmbHhaJarjwGdiVNIgcF7s7Qk\n"
        "NYNcgsc1l0KuNHvredTnYwPhv3C08IBfjtd2H9u0A+AWl5RlR4GDfv2Jzbe/F8U+\n"
        "0gxj8D2XWHlkbHIXKVk6jxj64xyNE1xB0Sv7gsDWpkaK6aw/zdsyxqiji4mThcYE\n"
        "cRSl4y9CGZREaiyD8dk/uiqKfQ26c1gfOUDYS2fKjH5NKh4J80wQj0GvS6nHiDH4\n"
        "-----END RSA PRIVATE KEY-----\n";

static const char *private_key_password ="CHAM569";

static const char *revoked_identity_certificate_filename = "revoked_identity_certificate";
static const char *revoked_identity_certificate =
        "data:,-----BEGIN CERTIFICATE-----\n"
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

static const char *revoked_private_key_filename = "revoked_private_key";
static const char *revoked_private_key =
        "data:,-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIEowIBAAKCAQEAxQJq8im5z1/RZcXS5OKM4PvjH5PXoEIPuIHEZXPH8EqpjB3W\n"
        "i9pUIE27nd8Gm3+i4IUBrOUx/LUtmI+k2sd87LxdNsuYm/yVExhAKbYRGW8P90XO\n"
        "AbivMILgjYPitKJazhvKEQLTkMVKO8pcBVtl6KWy15gyLd6eCxXfXPfdJkrO1zYm\n"
        "x4FXttUq7z/gJBRkbV2fb5Tb5lX/8VbjynvYYGGFAexH04XxnHnbrHY/4MoPnTYc\n"
        "ZqyEaALrT3Lcv2UrJJTw0mpUCrIy9LReKVIeOGrd9wE0jt3qk41EFZ2RWo8CIL3G\n"
        "fYqo1QtEzAbzsAAXL9S1HUN0OWJV+NoUqqzSvwIDAQABAoIBABU7YHk+w/a0deXI\n"
        "/ySJwfMRUnX5wfhUhks1OQxSAQ9FjKY8JP4nhn+AwSKPga/KfqxByV9vyAZbJFHX\n"
        "0UV+0FjXKBiaspTFEO/g4jFcnNUn4gmdLUmENOU+haLavtkG0lB6MDnLGy/0Az8U\n"
        "XPx60C3VhcO0dFv7LP822T60u9G/d8M034D8ruHsrLbpESUBORREXXDHAztH45nC\n"
        "Z95Icy3iC6dLyM+6e5UULus3lmY53+xWIMrRqIsS6mKmh84nNxwtbqcCwVrUY7cH\n"
        "XYgf8hqRFiSQiK+qtYjigpCTSM9B8ivDaxX5tdxuj/VuvVy+YPNcyq5+hrYPnYPO\n"
        "G5XsJckCgYEA56rtOgjGczZjjs0e9/R1v3hhy4dy+jGkJGrzLR7BwDLXO6+EwK94\n"
        "BcwQ16dDDxeRMaEGYeYc5egP/ODyAl+4Rd6auQkff8BfQK0UnPo6yPce0K6Yl56V\n"
        "MCf8BV63Vedex1riIVRBOx0PwQBN5h9Vr7/oKaC5ITS6kehL2uxLpZsCgYEA2bOZ\n"
        "+sbqKTylIJ/oSjoLhZ+S6I+2vY8IFtW65ZVaIveG0twxX6a1LnwmCNzXNs86mfs+\n"
        "ASVjUHk6GbAcOEKkZ5jQeDbYyd3jOAljWYqFQaoArB8+7AS2SkrM0NoimlH/I1Uv\n"
        "vVKT2JB+/5LK5KBxEz+6KZPpNPMZzwgfxa90y60CgYB0Hzc9ybw/b9nDcIm/W+fR\n"
        "i7PpYwF863kNUBaIXUxc3J8KKdZvBwUwUrN2hT6VyAhdSgt68u81RncNGGv2SKiD\n"
        "TStc6HfDf1e/gYI9lSf2J/hoPbv68+Bv/PrUbj+TbaASaTnD3wm7abvF0DM70CUR\n"
        "LS5f/1IMlPOXw0qSd7MLVQKBgQDSvFjBuOvTHzF5c1GZCLc+kknTdcqflGVwNVTG\n"
        "CN1IG/QXCa+BuA6LAQKQcbajB9biV6Kd2WNZ8v+a/i9TBq++2N50gCM6xd+9ztit\n"
        "RLnZ5obgFx8BuU38fIvnYEE+wUEJIt0jl1wmtzk4jRB6YBUVXQsIVHXbG7hQAL1A\n"
        "z6dvwQKBgHE7+xaHXeCBQxKsYeJWx9/WlUSGqcdOLOKrv4zo9PrpT2wb2pqGdVof\n"
        "ANjJA1V0bMk6+kirzJSzQ9oBql5InF/fNKaMtSj1+bjr0MY3NaQ57NOLAWWlFFrD\n"
        "cZ5yszaCk+uX7DACzN1XO7fL2FARNZkfHDZw/tQOKGDkl4x7IGml\n"
        "-----END RSA PRIVATE KEY-----\n";

static const char *ec_identity_certificate_filename = "ec_identity_certificate";
static const char *ec_identity_certificate =
        "data:,-----BEGIN CERTIFICATE-----\n"
        "MIICOzCCAeGgAwIBAgICEAAwCgYIKoZIzj0EAwIwZTELMAkGA1UEBhMCTkwxEzAR\n"
        "BgNVBAgMClNvbWUtU3RhdGUxHzAdBgNVBAoMFkFETElOSyBUZWNobm9sb2d5IEIu\n"
        "Vi4xIDAeBgNVBAMMF0NIQU1fNTcwIENBIGNlcnRpZmljYXRlMB4XDTE5MDIxODEw\n"
        "NTI0MVoXDTQ2MDcwNjEwNTI0MVowazELMAkGA1UEBhMCTkwxEzARBgNVBAgMClNv\n"
        "bWUtU3RhdGUxHzAdBgNVBAoMFkFETElOSyBUZWNobm9sb2d5IEIuVi4xJjAkBgNV\n"
        "BAMMHUNIQU1fNTcwIElkZW50aXR5IGNlcnRpZmljYXRlMFkwEwYHKoZIzj0CAQYI\n"
        "KoZIzj0DAQcDQgAEnbV79f5j2iTkDCbFMlVVs396YOoNViwKheBbhVoBG2n8I3mY\n"
        "M9Zg1dmrHh16HsJfrTCbc0VAOdkH91mNRPZr46N7MHkwCQYDVR0TBAIwADAsBglg\n"
        "hkgBhvhCAQ0EHxYdT3BlblNTTCBHZW5lcmF0ZWQgQ2VydGlmaWNhdGUwHQYDVR0O\n"
        "BBYEFMZoftcgs1FL1FBUJhKGvpvqVaHqMB8GA1UdIwQYMBaAFHUuWr3OtGtMktK9\n"
        "QnKSSydxn4ewMAoGCCqGSM49BAMCA0gAMEUCIQCyp777C9Tih7Asybj6ELAYS9xq\n"
        "vFhV6CJGk9ixW1AXdwIgKs9CEPx+Ajk3RErPm6OaVcsVLRKGBn7UuCR6VxNItWk=\n"
        "-----END CERTIFICATE-----\n";

static const char *ec_private_key_filename = "ec_private_key";
static const char *ec_private_key =
        "data:,-----BEGIN PRIVATE KEY-----\n"
        "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgP3SnBXzcCc0uUEiG\n"
        "0CPNdcV0hBewOnVoh4d9q9E5U5ihRANCAASdtXv1/mPaJOQMJsUyVVWzf3pg6g1W\n"
        "LAqF4FuFWgEbafwjeZgz1mDV2aseHXoewl+tMJtzRUA52Qf3WY1E9mvj\n"
        "-----END PRIVATE KEY-----\n";

static const char *ec_password ="CHAM-570";

static const char *ec_private_key_w_password_filename = "ec_private_key_w_password";
static const char *ec_private_key_w_password =
        "data:,-----BEGIN EC PRIVATE KEY-----\n"
        "Proc-Type: 4,ENCRYPTED\n"
        "DEK-Info: AES-256-CBC,11055B75D406068EB1FF850646228EA9\n"
        "\n"
        "GUnwN8e2gvUkopN3ak+2dK1dSTSKSJguers3h5C+qQDq57By933ijCCjUTu2LY/F\n"
        "ERH6m8UD6H5ij/QDsXLx6tH/dFQ7An+Zao3eD2N2zquGED/OfTQJFv3gBKs4RUtg\n"
        "66dfuv9mNSXt7Rnu9uBNtodm5JGifczdmIPHn0mNY2g=\n"
        "-----END EC PRIVATE KEY-----";

static const char *ec_identity_ca_filename = "ec_identity_ca";
static const char *ec_identity_ca =
        "data:,-----BEGIN CERTIFICATE-----\n"
        "MIICEDCCAbegAwIBAgIJAPOifu8ejrRRMAoGCCqGSM49BAMCMGUxCzAJBgNVBAYT\n"
        "Ak5MMRMwEQYDVQQIDApTb21lLVN0YXRlMR8wHQYDVQQKDBZBRExJTksgVGVjaG5v\n"
        "bG9neSBCLlYuMSAwHgYDVQQDDBdDSEFNXzU3MCBDQSBjZXJ0aWZpY2F0ZTAeFw0x\n"
        "OTAyMTgxMDQwMTZaFw00NjA3MDYxMDQwMTZaMGUxCzAJBgNVBAYTAk5MMRMwEQYD\n"
        "VQQIDApTb21lLVN0YXRlMR8wHQYDVQQKDBZBRExJTksgVGVjaG5vbG9neSBCLlYu\n"
        "MSAwHgYDVQQDDBdDSEFNXzU3MCBDQSBjZXJ0aWZpY2F0ZTBZMBMGByqGSM49AgEG\n"
        "CCqGSM49AwEHA0IABMXCYXBHEryADoYXMEE0Jw9aHlA7p3KVFzuypxuez0n7rKoX\n"
        "k9kanNtrw5o2X4WSWKM7zkH4I6AU7xSAQgJN+8GjUDBOMB0GA1UdDgQWBBR1Llq9\n"
        "zrRrTJLSvUJykksncZ+HsDAfBgNVHSMEGDAWgBR1Llq9zrRrTJLSvUJykksncZ+H\n"
        "sDAMBgNVHRMEBTADAQH/MAoGCCqGSM49BAMCA0cAMEQCIHKRM3VeB2F7z3nJT752\n"
        "gY5mNdj91ulmNX84TXA7UHNKAiA2ytpsV4OKURHkjyn1gnW48JDKtHGZF6/tMNvX\n"
        "VrDITA==\n"
        "-----END CERTIFICATE-----\n";

static const char *ec_identity_certificate_unsupported =
        "data:,-----BEGIN CERTIFICATE-----\n"
        "MIICFTCCAbygAwIBAgICEAEwCgYIKoZIzj0EAwIwWjELMAkGA1UEBhMCTkwxEzAR\n"
        "BgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMgUHR5\n"
        "IEx0ZDETMBEGA1UEAwwKQ0hBTTUwMF9DQTAeFw0xODAyMTkxMDMyMjRaFw0xOTAy\n"
        "MTkxMDMyMjRaMGExCzAJBgNVBAYTAk5MMRMwEQYDVQQIDApTb21lLVN0YXRlMSEw\n"
        "HwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQxGjAYBgNVBAMMEUNIQU01\n"
        "NjkgdW5zdXAga2V5MEkwEwYHKoZIzj0CAQYIKoZIzj0DAQIDMgAEKt3HYPnDlEOS\n"
        "zYqTzT2patyreLHN2Jty22KXwjaNAjgrwujdPr+MW38DsyBF5Yn9o3sweTAJBgNV\n"
        "HRMEAjAAMCwGCWCGSAGG+EIBDQQfFh1PcGVuU1NMIEdlbmVyYXRlZCBDZXJ0aWZp\n"
        "Y2F0ZTAdBgNVHQ4EFgQUG9MuQz3W/AKA98AyOKhI2af9I+0wHwYDVR0jBBgwFoAU\n"
        "ACsYsaEsZfjfRVrj0IBmcsncVyMwCgYIKoZIzj0EAwIDRwAwRAIgfhisahVmgghI\n"
        "GaaQavdKHpM/OTVODZPzYjky6Am+z08CIBidnuuznXrZtr78oy/tAES/7Lz8P5Iw\n"
        "Q1y5Vo8CdXQQ\n"
        "-----END CERTIFICATE-----\n";

static const char *ec_private_key_unsupported =
        "data:,-----BEGIN PRIVATE KEY-----\n"
        "MG8CAQAwEwYHKoZIzj0CAQYIKoZIzj0DAQIEVTBTAgEBBBh8p6kwBS7jT86ctN33\n"
        "Vs4vosHh7upPZBWhNAMyAAQq3cdg+cOUQ5LNipPNPalq3Kt4sc3Ym3LbYpfCNo0C\n"
        "OCvC6N0+v4xbfwOzIEXlif0=\n"
        "-----END PRIVATE KEY-----\n";


const char *crl_filename = "crl";
const char *crl =
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


CU_Init(ddssec_builtin_validate_local_identity)
{
    /* Only need the authentication plugin. */
    plugins = load_plugins(NULL   /* Access Control */,
                           &auth  /* Authentication */,
                           NULL   /* Cryptograpy    */,
                           NULL);
    return plugins ? 0 : -1;
}


CU_Clean(ddssec_builtin_validate_local_identity)
{
    unload_plugins(plugins);
    return 0;
}


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
print_guid(
    const char *msg,
    DDS_Security_GUID_t *guid)
{
    uint32_t i, j;

    printf("%s=", msg);
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            printf("%02x", guid->prefix[i*4+j]);
        }
        printf(":");
    }
    for (i = 0; i < 3; i++) {
        printf("%02x", guid->entityId.entityKey[i]);
    }
    printf(":%02x\n", guid->entityId.entityKind);
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
fill_participant_qos(
    DDS_Security_Qos *participant_qos,
    bool is_file_certificate, const char *certificate,
    bool is_file_ca, const char *ca,
    bool is_file_private_key, const char *private_key_data,
    const char *password,
    const char *trusted_ca_dir,
    bool is_file_crl, const char *crl_data)
{
    char identity_cert_path[1024];
    char identity_CA_path[1024];
    char private_key_path[1024];
    char trusted_ca_dir_path[1024];
    char crl_path[1024];
    unsigned size = 3;
    unsigned offset = 0;
    DDS_Security_Property_t *valbuf;

    password ? size++ : size;
    trusted_ca_dir ? size++ : size;
    crl_data ? size++ : size;

    memset(participant_qos, 0, sizeof(*participant_qos));
    dds_security_property_init(&participant_qos->property.value, size);

    valbuf = &participant_qos->property.value._buffer[offset++];
    valbuf->name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
    if (is_file_certificate) {
#ifdef WIN32
        snprintf(identity_cert_path, 1024, "file:%s\\validate_local_identity\\etc\\%s", CONFIG_ENV_TESTS_DIR, certificate);
#else
        snprintf(identity_cert_path, 1024, "file:%s/validate_local_identity/etc/%s", CONFIG_ENV_TESTS_DIR, certificate);
#endif
        valbuf->value = ddsrt_strdup(identity_cert_path);
    }
    else {
        valbuf->value = ddsrt_strdup(certificate);
    }

    valbuf = &participant_qos->property.value._buffer[offset++];
    valbuf->name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
    if (is_file_ca) {
#ifdef WIN32
        snprintf(identity_CA_path, 1024, "file:%s\\validate_local_identity\\etc\\%s", CONFIG_ENV_TESTS_DIR, ca);
#else
        snprintf(identity_CA_path, 1024, "file:%s/validate_local_identity/etc/%s", CONFIG_ENV_TESTS_DIR, ca);
#endif
        valbuf->value = ddsrt_strdup(identity_CA_path);
    }
    else {
        valbuf->value = ddsrt_strdup(ca);
    }

    valbuf = &participant_qos->property.value._buffer[offset++];
    valbuf->name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
    if (is_file_private_key) {
#ifdef WIN32
        snprintf(private_key_path, 1024, "file:%s\\validate_local_identity\\etc\\%s", CONFIG_ENV_TESTS_DIR, private_key_data);
#else
        snprintf(private_key_path, 1024, "file:%s/validate_local_identity/etc/%s", CONFIG_ENV_TESTS_DIR, private_key_data);
#endif
        valbuf->value = ddsrt_strdup(private_key_path);
    }
    else {
        valbuf->value = ddsrt_strdup(private_key_data);
    }

    if (password) {
        valbuf = &participant_qos->property.value._buffer[offset++];
        valbuf->name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PASSWORD);
        valbuf->value = ddsrt_strdup(password);
    }

    if (crl_data) {
      valbuf = &participant_qos->property.value._buffer[offset++];
      valbuf->name = ddsrt_strdup(ORG_ECLIPSE_CYCLONEDDS_SEC_AUTH_CRL);
      if (is_file_crl) {
#ifdef WIN32
          snprintf(crl_path, 1024, "file:%s\\validate_local_identity\\etc\\%s", CONFIG_ENV_TESTS_DIR, crl_data);
#else
          snprintf(crl_path, 1024, "file:%s/validate_local_identity/etc/%s", CONFIG_ENV_TESTS_DIR, crl_data);
#endif
          valbuf->value = ddsrt_strdup(crl_path);
      }
      else {
        valbuf->value = ddsrt_strdup(crl_data);
      }
    }

    if (trusted_ca_dir) {
        valbuf = &participant_qos->property.value._buffer[offset++];
#ifdef WIN32
        snprintf(trusted_ca_dir_path, 1024, "%s\\validate_local_identity\\etc\\%s", CONFIG_ENV_TESTS_DIR, trusted_ca_dir);
#else
        snprintf(trusted_ca_dir_path, 1024, "%s/validate_local_identity/etc/%s", CONFIG_ENV_TESTS_DIR, trusted_ca_dir);
#endif
        valbuf->name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR);
        valbuf->value = ddsrt_strdup(trusted_ca_dir_path);
    }
}


CU_Test(ddssec_builtin_validate_local_identity,happy_day)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};
    DDS_Security_boolean success;

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    print_guid("adjusted_participant_guid", &adjusted_participant_guid);
    CU_ASSERT (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    /* validate with file */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, true, identity_certificate_filename,
                                           true, identity_ca_filename,
                                           true, private_key_filename,
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    print_guid("adjusted_participant_guid", &adjusted_participant_guid);
    CU_ASSERT (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,invalid_certificate)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, invalid_identity_certificate,
                                           false, identity_ca,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    /* test with file */

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, true, invalid_identity_certificate_filename,
                                           false, identity_ca,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,invalid_root)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, invalid_identity_ca,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    /* test with file */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           true, invalid_identity_ca_filename,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,invalid_chain)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, unrelated_identity_ca,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    /* test with file input*/
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           true, unrelated_identity_ca_filename,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);


}

CU_Test(ddssec_builtin_validate_local_identity,certificate_key_too_small)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate_1024key,
                                           false, identity_ca,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,invalid_private_key)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           false, invalid_private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    /*test with file input */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           true, invalid_private_key_filename,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,private_key_too_small)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           false, private_key_1024,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,missing_certificate_property)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    memset(&participant_qos, 0, sizeof(participant_qos));
    dds_security_property_init(&participant_qos.property.value, 3);
    participant_qos.property.value._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_PREFIX "auth.identity_cert"); // deliberately *not* DDS_SEC_PROP_AUTH_IDENTITY_CERT
    participant_qos.property.value._buffer[0].value = ddsrt_strdup(identity_certificate);
    participant_qos.property.value._buffer[1].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
    participant_qos.property.value._buffer[1].value = ddsrt_strdup(identity_ca);
    participant_qos.property.value._buffer[2].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
    participant_qos.property.value._buffer[2].value = ddsrt_strdup(private_key_1024);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT_FATAL (exception.message != NULL);
    assert(exception.message != NULL); // for Clang's static analyzer
    CU_ASSERT(strcmp(exception.message, "validate_local_identity: missing property '" DDS_SEC_PROP_AUTH_IDENTITY_CERT "'") == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,missing_ca_property)
{
    DDS_Security_ValidationResult_t result;

    /* Dummy (even un-initialized) data for now. */
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    memset(&participant_qos, 0, sizeof(participant_qos));
    dds_security_property_init(&participant_qos.property.value, 3);
    participant_qos.property.value._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
    participant_qos.property.value._buffer[0].value = ddsrt_strdup(identity_certificate);
    participant_qos.property.value._buffer[1].name = ddsrt_strdup(DDS_SEC_PROP_PREFIX "auth.identit_ca"); // deliberately *not* DDS_SEC_PROP_AUTH_IDENTITY_CA
    participant_qos.property.value._buffer[1].value = ddsrt_strdup(identity_ca);
    participant_qos.property.value._buffer[2].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
    participant_qos.property.value._buffer[2].value = ddsrt_strdup(private_key_1024);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT_FATAL (exception.message != NULL);
    assert(exception.message != NULL); // for Clang's static analyzer
    CU_ASSERT(strcmp(exception.message, "validate_local_identity: missing property '" DDS_SEC_PROP_AUTH_IDENTITY_CA "'") == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}

CU_Test(ddssec_builtin_validate_local_identity,missing_private_key_property)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    memset(&participant_qos, 0, sizeof(participant_qos));
    dds_security_property_init(&participant_qos.property.value, 2);
    participant_qos.property.value._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
    participant_qos.property.value._buffer[0].value = ddsrt_strdup(identity_certificate);
    participant_qos.property.value._buffer[1].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
    participant_qos.property.value._buffer[1].value = ddsrt_strdup(identity_ca);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT_FATAL (exception.message != NULL);
    assert(exception.message != NULL); // for Clang's static analyzer
    CU_ASSERT(strcmp(exception.message, "validate_local_identity: missing property '" DDS_SEC_PROP_AUTH_PRIV_KEY "'") == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}

CU_Test(ddssec_builtin_validate_local_identity,unsupported_certification_format)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};
    char *cert;
    size_t len;

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    len = strlen("uri:") + strlen(&identity_certificate[6]) + 1;
    cert = ddsrt_malloc(len);

    snprintf(cert, len, "uri:%s", &identity_certificate[6]);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, cert,
                                           false, identity_ca,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    ddsrt_free(cert);
}

CU_Test(ddssec_builtin_validate_local_identity,encrypted_key)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           false, private_key_w_password,
                                           private_key_password,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    print_guid("adjusted_participant_guid", &adjusted_participant_guid);
    CU_ASSERT (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    /* test with file  */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           true, private_key_w_password_filename,
                                           private_key_password,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    print_guid("adjusted_participant_guid", &adjusted_participant_guid);
    CU_ASSERT (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}

CU_Test(ddssec_builtin_validate_local_identity,encrypted_key_no_password)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           false, private_key_w_password,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    /*test with file */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           true, private_key_w_password_filename,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}

CU_Test(ddssec_builtin_validate_local_identity,encrypted_key_invalid_password)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           false, private_key_w_password,
                                           "invalid",
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    /*test with file */

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           true, private_key_w_password_filename,
                                           "invalid",
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}

CU_Test(ddssec_builtin_validate_local_identity,happy_day_elliptic)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};
    DDS_Security_boolean success;

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, ec_identity_certificate,
                                           false, ec_identity_ca,
                                           false, ec_private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    print_guid("adjusted_participant_guid", &adjusted_participant_guid);
    CU_ASSERT (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    /* test with file */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, true, ec_identity_certificate_filename,
                                           true, ec_identity_ca_filename,
                                           true, ec_private_key_filename,
                                           NULL,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    print_guid("adjusted_participant_guid", &adjusted_participant_guid);
    CU_ASSERT (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,encrypted_ec_key)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, ec_identity_certificate,
                                           false, ec_identity_ca,
                                           false, ec_private_key_w_password,
                                           ec_password,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    print_guid("adjusted_participant_guid", &adjusted_participant_guid);
    CU_ASSERT_FATAL (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    /* test with file  */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, ec_identity_certificate,
                                           false, ec_identity_ca,
                                           true, ec_private_key_w_password_filename,
                                           ec_password,
                                           NULL,
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    CU_ASSERT (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}

CU_Test(ddssec_builtin_validate_local_identity,elliptic_unsupported_certificate)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, ec_identity_certificate_unsupported,
                                           false, ec_identity_ca,
                                           false, ec_private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}

CU_Test(ddssec_builtin_validate_local_identity,elliptic_unsupported_private_key)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, ec_identity_certificate,
                                           false, ec_identity_ca,
                                           false, ec_private_key_unsupported,
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}

CU_Test(ddssec_builtin_validate_local_identity,return_freed_handle)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};
    DDS_Security_boolean success;

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    print_guid("adjusted_participant_guid", &adjusted_participant_guid);
    CU_ASSERT (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    CU_ASSERT_FATAL (success);

    if (!success) {
        printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    CU_ASSERT_FALSE (success);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    if (!success) {
        printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,no_file)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    /* validate with file */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, true, identity_certificate_filename,
                                           false, identity_ca,
                                           true, "invalid_filename",
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT_FATAL (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT_FATAL (local_identity_handle == DDS_SECURITY_HANDLE_NIL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, true, identity_certificate_filename,
                                           true, "invalid_filename",
                                           true, private_key_filename,
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT_FATAL (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT_FATAL (local_identity_handle == DDS_SECURITY_HANDLE_NIL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);


    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, true, "invalid_filename",
                                           true, identity_ca_filename,
                                           false, private_key,
                                           NULL,
                                           NULL,
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT_FATAL (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT_FATAL (local_identity_handle == DDS_SECURITY_HANDLE_NIL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}

CU_Test(ddssec_builtin_validate_local_identity,with_extended_certificate_check)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};
    DDS_Security_boolean success;

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, identity_certificate,
                                           false, identity_ca,
                                           false, private_key,
                                           NULL,
                                           "trusted_ca_dir",
                                           false, NULL);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have succeeded. */
    CU_ASSERT_FATAL (result == DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle != DDS_SECURITY_HANDLE_NIL);

    print_guid("adjusted_participant_guid", &adjusted_participant_guid);
    CU_ASSERT (memcmp(&adjusted_participant_guid.entityId, &entityId, sizeof(entityId)) == 0);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    success = auth->return_identity_handle(auth, local_identity_handle, &exception);
    CU_ASSERT_TRUE (success);

    local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    reset_exception(&exception);

    /* validate with file */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, true, identity_certificate_filename,
                                           true, identity_ca_filename,
                                           true, private_key_filename,
                                           NULL,
                                           "trusted_ca_dir_not_matching",
                                           false, NULL);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    /* We expected the validation to have failed. */
    CU_ASSERT_FATAL (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (local_identity_handle == DDS_SECURITY_HANDLE_NIL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,crl)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, false, revoked_identity_certificate,
                                           false, identity_ca,
                                           false, revoked_private_key,
                                           NULL,
                                           NULL,
                                           false, crl);
    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT_FATAL (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    /* validate with file */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, true, revoked_identity_certificate_filename,
                                           true, identity_ca_filename,
                                           true, revoked_private_key_filename,
                                           NULL,
                                           NULL,
                                           true, crl_filename);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT_FATAL (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}


CU_Test(ddssec_builtin_validate_local_identity,trusted_ca_dir_and_crl)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
    DDS_Security_GUID_t adjusted_participant_guid;
    DDS_Security_DomainId domain_id = 0;
    DDS_Security_Qos participant_qos;
    DDS_Security_GUID_t candidate_participant_guid;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GuidPrefix_t prefix = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb};
    DDS_Security_EntityId_t entityId = {{0xa0,0xa1,0xa2},0x1};

    /* Check if we actually have the validate_local_identity() function. */
    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (auth->validate_local_identity != NULL);
    assert (auth->validate_local_identity != 0);

    /* validate with file */
    memset(&adjusted_participant_guid, 0, sizeof(adjusted_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    fill_participant_qos(&participant_qos, true, revoked_identity_certificate_filename,
                                           true, identity_ca_filename,
                                           true, revoked_private_key_filename,
                                           NULL,
                                           "trusted_ca_dir",
                                           true, crl_filename);

    /* Now call the function. */
    result = auth->validate_local_identity(
                            auth,
                            &local_identity_handle,
                            &adjusted_participant_guid,
                            domain_id,
                            &participant_qos,
                            &candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    /* We expected the validation to have failed. */
    CU_ASSERT_FATAL (result != DDS_SECURITY_VALIDATION_OK);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);
}
