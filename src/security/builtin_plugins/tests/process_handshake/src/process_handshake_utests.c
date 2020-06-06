#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/environ.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/openssl_support.h"
#include "common/src/handshake_helper.h"
#include "common/src/loader.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "config_env.h"

#define HANDSHAKE_SIGNATURE_SIZE 6

static const char * AUTH_PROTOCOL_CLASS_ID          = "DDS:Auth:PKI-DH:1.0";
static const char * PERM_ACCESS_CLASS_ID            = "DDS:Access:Permissions:1.0";

static const char * PROPERTY_IDENTITY_CA            = "dds.sec.auth.identity_ca";
static const char * PROPERTY_PRIVATE_KEY            = "dds.sec.auth.private_key";
static const char * PROPERTY_IDENTITY_CERT          = "dds.sec.auth.identity_certificate";
static const char * PROPERTY_TRUSTED_CA_DIR          = "dds.sec.auth.trusted_ca_dir";

static const char * PROPERTY_CERT_SUBJECT_NAME      = "dds.cert.sn";
static const char * PROPERTY_CERT_ALGORITHM         = "dds.cert.algo";
static const char * PROPERTY_CA_SUBJECT_NAME        = "dds.ca.sn";
static const char * PROPERTY_CA_ALGORITHM           = "dds.ca.aglo";

static const char * PROPERTY_PERM_CA_SUBJECT_NAME   = "ds.perm_ca.sn";

static const char * SUBJECT_NAME_IDENTITY_CERT      = "CN=CHAM-574 client,O=Some Company,ST=Some-State,C=NL";
static const char * SUBJECT_NAME_IDENTITY_CA        = "CN=CHAM-574 authority,O=Some Company,ST=Some-State,C=NL";

static const char * RSA_2048_ALGORITHM_NAME         = "RSA-2048";

static const char * AUTH_REQUEST_TOKEN_CLASS_ID         = "DDS:Auth:PKI-DH:1.0+AuthReq";
static const char * AUTH_REQUEST_TOKEN_FUTURE_PROP_NAME = "future_challenge";

static const char * AUTH_HANDSHAKE_REQUEST_TOKEN_CLASS_ID = "DDS:Auth:PKI-DH:1.0+Req";
static const char * AUTH_HANDSHAKE_REPLY_TOKEN_CLASS_ID   = "DDS:Auth:PKI-DH:1.0+Reply";
static const char * AUTH_HANDSHAKE_FINAL_TOKEN_CLASS_ID   = "DDS:Auth:PKI-DH:1.0+Final";

typedef enum {
    HANDSHAKE_REQUEST,
    HANDSHAKE_REPLY,
    HANDSHAKE_FINAL
} HandshakeStep_t;


struct octet_seq {
    unsigned char  *data;
    uint32_t  length;
};

static const char * AUTH_DSIGN_ALGO_RSA_NAME   = "RSASSA-PSS-SHA256";
static const char * AUTH_KAGREE_ALGO_RSA_NAME  = "DH+MODP-2048-256";
static const char * AUTH_KAGREE_ALGO_ECDH_NAME = "ECDH+prime256v1-CEUM";



static const char *identity_certificate =

"data:,-----BEGIN CERTIFICATE-----\n"
"MIIDYDCCAkigAwIBAgIBBDANBgkqhkiG9w0BAQsFADByMQswCQYDVQQGEwJOTDEL\n"
"MAkGA1UECBMCT1YxEzARBgNVBAoTCkFETGluayBJU1QxGTAXBgNVBAMTEElkZW50\n"
"aXR5IENBIFRlc3QxJjAkBgkqhkiG9w0BCQEWF2luZm9AaXN0LmFkbGlua3RlY2gu\n"
"Y29tMB4XDTE4MDMxMjAwMDAwMFoXDTI3MDMxMTIzNTk1OVowdTELMAkGA1UEBhMC\n"
"TkwxCzAJBgNVBAgTAk9WMRAwDgYDVQQKEwdBRExpbmsgMREwDwYDVQQLEwhJU1Qg\n"
"VGVzdDETMBEGA1UEAxMKQWxpY2UgVGVzdDEfMB0GCSqGSIb3DQEJARYQYWxpY2VA\n"
"YWRsaW5rLmlzdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANBW+tEZ\n"
"Baw7EQCEXyzH9n7IkZ8PQIKe8hG1LAOGYOF/oUYQZJO/HxbWoC4rFqOC20+A6is6\n"
"kFwr1Zzp/Wurk9CrFXo5Nomi6ActH6LUM57nYqN68w6U38z/XkQxVY/ESZ5dySfD\n"
"9Q1C8R+zdE8gwbimdYmwX7ioz336nghM2CoAHPDRthQeJupl8x4V7isOltr9CGx8\n"
"+imJXbGr39OK6u87cNLeu23sUkOIC0lSRMIqIQK3oJtHS70J2qecXdqp9MhE7Xky\n"
"/GPlI8ptQ1gJ8A3cAOvtI9mtMJMszs2EKWTLfeTcmfJHKKhKjvCgDdh3Jan4x5YP\n"
"Yg7HG6H+ceOUkMMCAwEAATANBgkqhkiG9w0BAQsFAAOCAQEAkvuqZzyJ3Nu4/Eo5\n"
"kD0nVgYGBUl7cspu+636q39zPSrxLEDMUWz+u8oXLpyGcgiZ8lZulPTV8dmOn+3C\n"
"Vg55c5C+gbnbX3MDyb3wB17296RmxYf6YNul4sFOmj6+g2i+Dw9WH0PBCVKbA84F\n"
"jR3Gx2Pfoifor3DvT0YFSsjNIRt090u4dQglbIb6cWEafC7O24t5jFhGPvJ7L9SE\n"
"gB0Drh/HmKTVuaqaRkoOKkKaKuWoXsszK1ZFda1DHommnR5LpYPsDRQ2fVM4EuBF\n"
"By03727uneuG8HLuNcLEV9H0i7LxtyfFkyCPUQvWG5jehb7xPOz/Ml26NAwwjlTJ\n"
"xEEFrw==\n"
"-----END CERTIFICATE-----\n";


static const char *identity_ca =
"data:,-----BEGIN CERTIFICATE-----\n"
"MIIEKTCCAxGgAwIBAgIBATANBgkqhkiG9w0BAQsFADByMQswCQYDVQQGEwJOTDEL\n"
"MAkGA1UECBMCT1YxEzARBgNVBAoTCkFETGluayBJU1QxGTAXBgNVBAMTEElkZW50\n"
"aXR5IENBIFRlc3QxJjAkBgkqhkiG9w0BCQEWF2luZm9AaXN0LmFkbGlua3RlY2gu\n"
"Y29tMB4XDTE4MDMxMjAwMDAwMFoXDTI3MDMxMTIzNTk1OVowcjELMAkGA1UEBhMC\n"
"TkwxCzAJBgNVBAgTAk9WMRMwEQYDVQQKEwpBRExpbmsgSVNUMRkwFwYDVQQDExBJ\n"
"ZGVudGl0eSBDQSBUZXN0MSYwJAYJKoZIhvcNAQkBFhdpbmZvQGlzdC5hZGxpbmt0\n"
"ZWNoLmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANa/ENFfGVXg\n"
"bPLTzBdDfiZQcp5dWZ//Pb8ErFOJu8uosVHFv8t69dgjHgNHB4OsjmjnR7GfKUZT\n"
"0cMvWJnjsC7DDlBwFET9rj4k40n96bbVCH9I7+tNhsoqzc6Eu+5h4sk7VfNGTM2Z\n"
"SyCd4GiSZRuA44rRbhXI7/LDpr4hY5J9ZDo5AM9ZyoLAoh774H3CZWD67S35XvUs\n"
"72dzE6uKG/vxBbvZ7eW2GLO6ewa9UxlnLVMPfJdpkp/xYXwwcPW2+2YXCge1ujxs\n"
"tjrOQJ5HUySh6DkE/kZpx8zwYWm9AaCrsvCIX1thsqgvKy+U5v1FS1L58eGc6s//\n"
"9yMgNhU29R0CAwEAAaOByTCBxjAMBgNVHRMEBTADAQH/MB0GA1UdDgQWBBRNVUJN\n"
"FzhJPReYT4QSx6dK53CXCTAfBgNVHSMEGDAWgBRNVUJNFzhJPReYT4QSx6dK53CX\n"
"CTAPBgNVHQ8BAf8EBQMDB/+AMGUGA1UdJQEB/wRbMFkGCCsGAQUFBwMBBggrBgEF\n"
"BQcDAgYIKwYBBQUHAwMGCCsGAQUFBwMEBggrBgEFBQcDCAYIKwYBBQUHAwkGCCsG\n"
"AQUFBwMNBggrBgEFBQcDDgYHKwYBBQIDBTANBgkqhkiG9w0BAQsFAAOCAQEAcOLF\n"
"ZYdJguj0uxeXB8v3xnUr1AWz9+gwg0URdfNLU2KvF2lsb/uznv6168b3/FcPgezN\n"
"Ihl9GqB+RvGwgXS/1UelCGbQiIUdsNxk246P4uOGPIyW32RoJcYPWZcpY+cw11tQ\n"
"NOnk994Y5/8ad1DmcxVLLqq5kwpXGWQufV1zOONq8B+mCvcVAmM4vkyF/de56Lwa\n"
"sAMpk1p77uhaDnuq2lIR4q3QHX2wGctFid5Q375DRscFQteY01r/dtwBBrMn0wuL\n"
"AMNx9ZGD+zAoOUaslpIlEQ+keAxk3jgGMWFMxF81YfhEnXzevSQXWpyek86XUyFL\n"
"O9IAQi5pa15gXjSbUg==\n"
"-----END CERTIFICATE-----\n";

static const char *remote_identity_certificate =
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


static const char *private_key =

"data:,-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEowIBAAKCAQEA0Fb60RkFrDsRAIRfLMf2fsiRnw9Agp7yEbUsA4Zg4X+hRhBk\n"
"k78fFtagLisWo4LbT4DqKzqQXCvVnOn9a6uT0KsVejk2iaLoBy0fotQznudio3rz\n"
"DpTfzP9eRDFVj8RJnl3JJ8P1DULxH7N0TyDBuKZ1ibBfuKjPffqeCEzYKgAc8NG2\n"
"FB4m6mXzHhXuKw6W2v0IbHz6KYldsavf04rq7ztw0t67bexSQ4gLSVJEwiohAreg\n"
"m0dLvQnap5xd2qn0yETteTL8Y+Ujym1DWAnwDdwA6+0j2a0wkyzOzYQpZMt95NyZ\n"
"8kcoqEqO8KAN2HclqfjHlg9iDscbof5x45SQwwIDAQABAoIBAG0dYPeqd0IhHWJ7\n"
"8azufbchLMN1pX/D51xG2uptssfnpHuhkkufSZUYi4QipRS2ME6PYhWJ8pmTi6lH\n"
"E6cUkbI0KGd/F4U2gPdhNrR9Fxwea5bbifkVF7Gx/ZkRjZJiZ3w9+mCNTQbJDKhh\n"
"wITAzzT6WYznhvqbzzBX1fTa6kv0GAQtX7aHKM+XIwkhX2gzU5TU80bvH8aMrT05\n"
"tAMGQqkUeRnpo0yucBl4VmTZzd/+X/d2UyXR0my15jE5iH5o+p+E6qTRE9D+MGUd\n"
"MQ6Ftj0Untqy1lcog1ZLL6zPlnwcD4jgY5VCYDgvabnrSwymOJapPLsAEdWdq+U5\n"
"ec44BMECgYEA/+3qPUrd4XxA517qO3fCGBvf2Gkr7w5ZDeATOTHGuD8QZeK0nxPl\n"
"CWhRjdgkqo0fyf1cjczL5XgYayo+YxkO1Z4RUU+8lJAHlVx9izOQo+MTQfkwH4BK\n"
"LYlHxMoHJwAOXXoE+dmBaDh5xT0mDUGU750r763L6EFovE4qRBn9hxkCgYEA0GWz\n"
"rpOPNxb419WxG9npoQYdCZ5IbmEOGDH3ReggVzWHmW8sqtkqTZm5srcyDpqAc1Gu\n"
"paUveMblEBbU+NFJjLWOfwB5PCp8jsrqRgCQSxolShiVkc3Vu3oyzMus9PDge1eo\n"
"9mwVGO7ojQKWRu/WVAakENPaAjeyyhv4dqSNnjsCgYEAlwe8yszqoY1k8+U0T0G+\n"
"HeIdOCXgkmOiNCj+zyrLvaEhuS6PLq1b5TBVqGJcSPWdQ+MrglbQIKu9pUg5ptt7\n"
"wJ5WU+i9PeK9Ruxc/g/BFKYFkFJQjtZzb+nqm3wpul8zGwDN/O/ZiTqCyd3rHbmM\n"
"/dZ/viKPCZHIEBAEq0m3LskCgYBndzcAo+5k8ZjWwBfQth5SfhCIp/daJgGzbYtR\n"
"P/BenAsY2KOap3tjT8Fsw5usuHSxzIojX6H0Gvu7Qzq11mLn43Q+BeQrRQTWeFRc\n"
"MQdy4iZFZXNNEp7dF8yE9VKHwdgSJPGUdxD6chMvf2tRCN6mlS171VLV6wVvZvez\n"
"H/vX5QKBgD2Dq/NHpjCpAsECP9awmNF5Akn5WJbRGmegwXIih2mOtgtYYDeuQyxY\n"
"ZCrdJFfIUjUVPagshEmUklKhkYMYpzy2PQDVtaVcm6UNFroxT5h+J+KDs1LN1H8G\n"
"LsASrzyAg8EpRulwXEfLrWKiu9DKv8bMEgO4Ovgz8zTKJZIFhcac\n"
"-----END RSA PRIVATE KEY-----\n";


static char *remote_private_key =
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEowIBAAKCAQEAweb6oblp8IcV2an3z5n484Hu8PdaF+CEpW6CHwV6d7C49pqi\n"
"0rGIYATSQJ/xZdTmXhcNHeIFYmwYMeWXXI7Ivgpb2CvB+d+IkyXza4jMoYOJElsW\n"
"RdU4sX8NxUuy6oyhGOWTT36eWuD/vFL3qK2CbrK55b9Q4hemj4SUIfCetiem4cet\n"
"gzSzu3WWzdAVLJ1rcg3H4uQzo39v695UkX3nJ97ga9UGEWkuRa1fjEqBnIyNzO+Z\n"
"notU01Ke2E+GVkUeaylDKpetOcbJrHerKiWiqH2JsO8ArmlSfOqW/8PLK3XXSrdt\n"
"cVHkDTCp+PR+q2nszDhN1OQVDLWfj9jgS5PwjwIDAQABAoIBAHfgWhED9VgL29le\n"
"uGMzmPLK4LM+6Qcb+kXghTeyhl1a928WeRVzRpG+SVJEz9QaBHYlICnaY2PO2kJ2\n"
"49YIPFkpRFDn9JuLs/7tFonj4Eb2cBbWE3YG9W7e0t+oBiv1117yB9m8uSAMPG7s\n"
"iEpTQvE3M7CzT8kHwCS4XXCCN0z7LqKyZ1heScjdfhV3D2TnFFjdtQ/9KfQa3hIc\n"
"6ftbpi4EKbfasspyqfrJ/cqjHzse9iEXLOZJhs+atBAKe/uJ4Hc3LRPbX4MPniAp\n"
"JJrldXFK9p+HILlbXvu+5n+DSGbZmT1x9a/E9suGyoJiASDH2Ax4yCVTi+v8C1R2\n"
"aKdU1LkCgYEA/3dFuM6zIHwiJ0GKT0gtJL6J3m+i51SNcRIm8deXt6HULMpUNajj\n"
"vZ1bgQm/h+uRBlPV3swkaVxvPTIabOTY4gmCBSzvVCSIAKHVc/+5Nkl9KruwSq4G\n"
"tctmXZ7ymMDi+6QGCJTJkAx6jptXyrzC00HOjXOwyQ+iDipqgr3A8FsCgYEAwk7B\n"
"2/hi569EIHFRT6nz/JMqQVPZ/MJDKoKhffTbnjQ5OAzpiVN6cyThMM1iVJEBFNhx\n"
"OEacy60Qj0TtR1oYrQSRSLm58TTxiuB4Pohbmg3iU+kSM/eTq/ups/Ul1oCs2eAb\n"
"POfweD3c4d4i7sN8bUNQXehiE4MOlK9TYQy39t0CgYAJht0mwy6S644qgJsz0bE9\n"
"SY3Cqc8daV3M9axWIIAb7QEImpMBXUcA7zlWWpK18ub5oW68XEiPVU8grRmnLfGY\n"
"nFoo70ANlz8rJt3a8ZJqn9r3GQC+CDdf2DH9E8xgPfE5CSjgcQwDPzPi1ZA0k02A\n"
"q1eUltfk55xXguVt8r2bOQKBgQC7+kldr1yv20VDRZ1uPnMGRLE6Zg6bkqw78gid\n"
"vEbDNK6uZP+BlTr/LgyVk/yu52Fucz6FPPrvqEw+7mXHA4ifya1r+BHFIn0S57os\n"
"dOp5jTkKCI9NqxQ3683vhRjH/dA7L63qLFDdYqvP74FID+LOKbMURn6rdbyjZ0J4\n"
"vz8yGQKBgHIzcKlQosRxf+KptOPMGRs30L9PnH+sNmTo2SmEzAGkBkt1msGRh/2l\n"
"uT3hOEhUXL9knRyXwQSXgrIwr9QwI5rGS5FAgX26TgBtPBDs2NuyyhhS5yxsiEPT\n"
"BR+EjQFW9dzRkpRJgvsG4DcNAhFn7fQqFNcWXgFWuBXmGNkdtEGR\n"
"-----END RSA PRIVATE KEY-----";

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

static struct plugins_hdl *plugins = NULL;
static dds_security_authentication *auth = NULL;
static DDS_Security_IdentityHandle local_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle remote_identity_handle1 = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle remote_identity_handle2 = DDS_SECURITY_HANDLE_NIL;
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

    token->class_id = ddsrt_strdup(AUTH_PROTOCOL_CLASS_ID);
    token->properties._maximum = 4;
    token->properties._length  = 4;
    token->properties._buffer = DDS_Security_PropertySeq_allocbuf(4);

    token->properties._buffer[0].name = ddsrt_strdup(PROPERTY_CERT_SUBJECT_NAME);
    token->properties._buffer[0].value = ddsrt_strdup(SUBJECT_NAME_IDENTITY_CERT);
    token->properties._buffer[0].propagate = true;

    token->properties._buffer[1].name = ddsrt_strdup(PROPERTY_CERT_ALGORITHM);
    token->properties._buffer[1].value = ddsrt_strdup(certAlgo);
    token->properties._buffer[1].propagate = true;

    token->properties._buffer[2].name = ddsrt_strdup(PROPERTY_CA_SUBJECT_NAME);
    token->properties._buffer[2].value = ddsrt_strdup(SUBJECT_NAME_IDENTITY_CA);
    token->properties._buffer[2].propagate = true;

    token->properties._buffer[3].name = ddsrt_strdup(PROPERTY_CA_ALGORITHM);
    token->properties._buffer[3].value = ddsrt_strdup(caAlgo);
    token->properties._buffer[3].propagate = true;
}

static void
initialize_permissions_token(
    DDS_Security_PermissionsToken *token,
    const char *caAlgo)
{
    token->class_id = ddsrt_strdup(PERM_ACCESS_CLASS_ID);
    token->properties._length = 2;
    token->properties._maximum = 2;
    token->properties._buffer = DDS_Security_PropertySeq_allocbuf(4);
    token->properties._buffer[0].name = ddsrt_strdup(PROPERTY_CERT_SUBJECT_NAME);
    token->properties._buffer[0].value = ddsrt_strdup(SUBJECT_NAME_IDENTITY_CA);
    token->properties._buffer[1].name = ddsrt_strdup(PROPERTY_PERM_CA_SUBJECT_NAME);
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

    token->class_id = ddsrt_strdup(AUTH_REQUEST_TOKEN_CLASS_ID);
    token->binary_properties._maximum = 1;
    token->binary_properties._length = 1;
    token->binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
    token->binary_properties._buffer->name = ddsrt_strdup(AUTH_REQUEST_TOKEN_FUTURE_PROP_NAME);

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
validate_local_identity(const char* trusted_ca_dir)
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
    size_t size;

    memset(&local_participant_guid, 0, sizeof(local_participant_guid));
    memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    memset(&participant_qos, 0, sizeof(participant_qos));
    if( trusted_ca_dir != NULL){
        char trusted_ca_dir_path[1024];
        dds_security_property_init(&participant_qos.property.value, 4);
#ifdef WIN32
    snprintf(trusted_ca_dir_path, 1024, "%s\\validate_begin_handshake_reply\\etc\\%s", CONFIG_ENV_TESTS_DIR, trusted_ca_dir);
#else
    snprintf(trusted_ca_dir_path, 1024, "%s/validate_begin_handshake_reply/etc/%s", CONFIG_ENV_TESTS_DIR, trusted_ca_dir);
#endif
        participant_qos.property.value._buffer[3].name = ddsrt_strdup(PROPERTY_TRUSTED_CA_DIR);
        participant_qos.property.value._buffer[3].value = ddsrt_strdup(trusted_ca_dir_path);
    }
    else{
        dds_security_property_init(&participant_qos.property.value, 3);
    }
    participant_qos.property.value._buffer[0].name = ddsrt_strdup(PROPERTY_IDENTITY_CERT);
    participant_qos.property.value._buffer[0].value = ddsrt_strdup(identity_certificate);
    participant_qos.property.value._buffer[1].name = ddsrt_strdup(PROPERTY_IDENTITY_CA);
    participant_qos.property.value._buffer[1].value = ddsrt_strdup(identity_ca);
    participant_qos.property.value._buffer[2].name = ddsrt_strdup(PROPERTY_PRIVATE_KEY);
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

    serializer_participant_data(local_participant_data, &sdata, &size);

    serialized_participant_data._length = serialized_participant_data._maximum = (DDS_Security_unsigned_long) size;
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
    int i, sz;

    name = X509_get_subject_name(cert);
    sz = i2d_X509_NAME(name, &tmp);
    if (sz > 0) {
        subject = ddsrt_malloc( (size_t)sz);
        memcpy(subject, tmp,  (size_t)sz);
        OPENSSL_free(tmp);

        SHA256(subject,  (size_t)sz, high);
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

    size = (uint32_t) i2d_ASN1_INTEGER(asn1int, &buffer);
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
        pubkey->length = (uint32_t) EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, pubkey->data, sz, NULL);
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

    challenge1_predefined_glb = find_binary_property(&g_remote_auth_request_token, AUTH_REQUEST_TOKEN_FUTURE_PROP_NAME);
    challenge2_predefined_glb = challenge1_predefined_glb;

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

    DDS_Security_ParticipantBuiltinTopicData_free(remote_participant_data1);
    DDS_Security_ParticipantBuiltinTopicData_free(remote_participant_data2);
    DDS_Security_ParticipantBuiltinTopicData_free(remote_participant_data3);
}

CU_Init(ddssec_builtin_process_handshake)
{
    int result = 0;
    dds_openssl_init ();

    /* Only need the authentication plugin. */
    plugins = load_plugins(NULL   /* Access Control */,
                           &auth  /* Authentication */,
                           NULL   /* Cryptograpy    */);
    if (plugins) {
        result = validate_local_identity( NULL );
        if (result >= 0) {
            result = validate_remote_identities( remote_identity_certificate );
        }
        if (result >= 0) {
            result = create_dh_key_modp_2048(&dh_modp_key);
        }
        if (result >= 0) {
            result = get_dh_public_key_modp_2048(dh_modp_key, &dh_modp_pub_key);
        }
        if (result >= 0) {
            result = create_dh_key_ecdh(&dh_ecdh_key);
        }
        if (result >= 0) {
            result = get_dh_public_key_ecdh(dh_ecdh_key, &dh_ecdh_pub_key);
        }
        if (result >= 0) {
            octet_seq_init(&invalid_dh_pub_key, dh_modp_pub_key.data, dh_modp_pub_key.length);
            invalid_dh_pub_key.data[0] = 0x08;
        }
    } else {
        result = -1;
    }


    return result;
}

CU_Clean(ddssec_builtin_process_handshake)
{
    release_local_identity();
    release_remote_identities();
    unload_plugins(plugins);
    octet_seq_deinit(&dh_modp_pub_key);
    octet_seq_deinit(&dh_ecdh_pub_key);
    octet_seq_deinit(&invalid_dh_pub_key);
    if (dh_modp_key) {
        EVP_PKEY_free(dh_modp_key);
    }
    if (dh_ecdh_key) {
        EVP_PKEY_free(dh_ecdh_key);
    }
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
           set_binary_property_string(c_id, "c.id", certificate);
       } else {
           set_binary_property_string(c_id, "c.idx", "rubbish");
       }

       /* Store the permission document in the c.perm property */
       set_binary_property_string(c_perm, "c.perm", "permissions_document");

       /* Store the provided local_participant_data in the c.pdata property */
       set_binary_property_value(c_pdata, "c.pdata", serialized_local_participant_data, (uint32_t)serialized_local_participant_data_size);
       ddsrt_free(serialized_local_participant_data);

       /* Set the used signing algorithm descriptor in c.dsign_algo */
       if (dsign) {
           set_binary_property_string(c_dsign_algo, "c.dsign_algo", dsign);
       } else {
           set_binary_property_string(c_dsign_algo, "c.dsign_algox", "rubbish");
       }

       /* Set the used key algorithm descriptor in c.kagree_algo */
       if (kagree) {
           set_binary_property_string(c_kagree_algo, "c.kagree_algo", kagree);
       } else {
           set_binary_property_string(c_kagree_algo, "c.kagree_algox", "rubbish");
       }

       /* Calculate the hash_c1 */
       {
           DDS_Security_BinaryPropertySeq bseq;
           DDS_Security_Serializer serializer;
           unsigned char hash1_sentrequest_arr[32];
           unsigned char *buffer;
           size_t size;

           bseq._length = bseq._maximum = 5;
           bseq._buffer = tokens;

           serializer = DDS_Security_Serializer_new(1024, 1024);

           DDS_Security_Serialize_BinaryPropertySeq(serializer, &bseq);
           DDS_Security_Serializer_buffer(serializer, &buffer, &size);
           SHA256(buffer, size, hash1_sentrequest_arr);
           ddsrt_free(buffer);
           DDS_Security_Serializer_free(serializer);

           set_binary_property_value(hash_c1, "hash_c1", hash1_sentrequest_arr, sizeof(hash1_sentrequest_arr));
       }

       /* Set the DH public key associated with the local participant in dh1 property */
       if (diffie_hellman1) {
           set_binary_property_value(dh1, "dh1", diffie_hellman1->data, diffie_hellman1->length);
       } else {
           set_binary_property_string(dh1, "dh1x", "rubbish");
       }

       /* Set the challenge in challenge1 property */
       if (challengeData) {
           set_binary_property_value(challenge1, "challenge1", challengeData, challengeDataSize);
       } else {
           set_binary_property_value(challenge1, "challenge1x", challenge1_predefined_glb->value._buffer, challenge1_predefined_glb->value._length);
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
           set_binary_property_string(c_id, "c.id", certificate);
        } else {
           set_binary_property_string(c_id, "c.idx", "rubbish");
        }

        /* Store the permission document in the c.perm property */
        set_binary_property_string(c_perm, "c.perm", "permissions_document");

        /* Store the provided local_participant_data in the c.pdata property */
        set_binary_property_value(c_pdata, "c.pdata", serialized_local_participant_data, (uint32_t )serialized_local_participant_data_size);
        ddsrt_free(serialized_local_participant_data);

        /* Set the used signing algorithm descriptor in c.dsign_algo */
        if (dsign) {
           set_binary_property_string(c_dsign_algo, "c.dsign_algo", dsign);
        } else {
           set_binary_property_string(c_dsign_algo, "c.dsign_algox", "rubbish");
        }

        /* Set the used key algorithm descriptor in c.kagree_algo */
        if (kagree) {
           set_binary_property_string(c_kagree_algo, "c.kagree_algo", kagree);
        } else {
           set_binary_property_string(c_kagree_algo, "c.kagree_algox", "rubbish");
        }

        CU_ASSERT_FATAL(hash1_from_request != NULL);
        assert(hash1_from_request != NULL); // for Clang's static analyzer

        set_binary_property_value(hash_c1, "hash_c1", hash1_from_request->value._buffer, hash1_from_request->value._length);

        /* Calculate the hash_c2 */
        {
           DDS_Security_BinaryPropertySeq bseq;
           DDS_Security_Serializer serializer;
           unsigned char hash2_sentreply_arr[32];
           unsigned char *buffer;
           size_t size;

           bseq._length = bseq._maximum = 5;
           bseq._buffer = tokens;

           serializer = DDS_Security_Serializer_new(1024, 1024);

           DDS_Security_Serialize_BinaryPropertySeq(serializer, &bseq);
           DDS_Security_Serializer_buffer(serializer, &buffer, &size);
           SHA256(buffer, size, hash2_sentreply_arr);

           ddsrt_free(buffer);
           DDS_Security_Serializer_free(serializer);

           set_binary_property_value(hash_c2, "hash_c2", hash2_sentreply_arr, sizeof(hash2_sentreply_arr));
        }

        /* Set the challenge in challenge1 property */
        if (challengeData) {
          set_binary_property_value(challenge1, "challenge1", challengeData, challengeDataSize);
        } else {
          set_binary_property_value(challenge1, "challenge1x", challenge2->value._buffer, challenge2->value._length);
        }

        /* Set the challenge in challenge2 property */
        if (challengeData2) {
           set_binary_property_value(challenge2, "challenge2", challengeData2, challengeDataSize2);
        } else {
           set_binary_property_value(challenge2, "challenge2x", challenge2->value._buffer, challenge2->value._length);
        }


        /* Set the DH public key associated with the local participant in dh1 property */
        if (diffie_hellman1) {
          set_binary_property_value(dh1, "dh1", diffie_hellman1->data, diffie_hellman1->length);
        } else {
          set_binary_property_string(dh1, "dh1x", "rubbish");
        }

        /* Set the DH public key associated with the local participant in dh2 property */
        if (diffie_hellman2) {
          set_binary_property_value(dh2, "dh2", diffie_hellman2->data, diffie_hellman2->length);
        } else {
          set_binary_property_string(dh2, "dh2x", "rubbish");
        }

        /* Calculate the signature */
        {
            BIO *bio;
            EVP_PKEY *private_key_x509;
            unsigned char *sign;
            size_t signlen;

            const DDS_Security_BinaryProperty_t * binary_properties[ HANDSHAKE_SIGNATURE_SIZE ];

            /* load certificate in buffer */
            bio = BIO_new_mem_buf((const char *) remote_private_key, -1);
            assert( bio );
            private_key_x509 = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
            assert (private_key_x509 );

            binary_properties[0] = hash_c2;
            binary_properties[1] = challenge2;
            binary_properties[2] = dh2;
            binary_properties[3] = challenge1;
            binary_properties[4] = dh1;
            binary_properties[5] = hash_c1;

            if (create_signature_for_test(private_key_x509, binary_properties, HANDSHAKE_SIGNATURE_SIZE , &sign, &signlen, &exception) != DDS_SECURITY_VALIDATION_OK)
            {
                printf("Exception: %s\n", exception.message);
            }
            else
            {
                set_binary_property_value(signature, "signature", sign, (uint32_t ) signlen);
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

        set_binary_property_value(hash_c1, "hash_c1", hash1_from_request->value._buffer, hash1_from_request->value._length);
        set_binary_property_value(hash_c2, "hash_c2", hash2_from_reply->value._buffer, hash2_from_reply->value._length);

        printf("process: %s\n", hash_c1->name);

        /* Set the challenge in challenge1 property */
        if (challengeData) {
          set_binary_property_value(challenge1, "challenge1", challengeData, challengeDataSize);
        } else {
          set_binary_property_value(challenge1, "challenge1x", challenge2->value._buffer, challenge2->value._length);
        }

        /* Set the challenge in challenge2 property */
        if (challengeData2) {
           set_binary_property_value(challenge2, "challenge2", challengeData2, challengeDataSize2);
        } else {
           set_binary_property_value(challenge2, "challenge2x", challenge2->value._buffer, challenge2->value._length);
        }


        /* Set the DH public key associated with the local participant in dh1 property */
        if (diffie_hellman1) {
          set_binary_property_value(dh1, "dh1", diffie_hellman1->data, diffie_hellman1->length);
        } else {
          set_binary_property_string(dh1, "dh1x", "rubbish");
        }

        /* Set the DH public key associated with the local participant in dh2 property */
        if (diffie_hellman2) {
          set_binary_property_value(dh2, "dh2", diffie_hellman2->data, diffie_hellman2->length);
        } else {
          set_binary_property_string(dh2, "dh2x", "rubbish");
        }

        /* Calculate the signature */
        {
            BIO *bio;
            EVP_PKEY *private_key_x509;
            unsigned char *sign;
            size_t signlen;
            const DDS_Security_BinaryProperty_t * binary_properties[ HANDSHAKE_SIGNATURE_SIZE ];

            /* load certificate in buffer */
            bio = BIO_new_mem_buf((const char *) remote_private_key, -1);
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
                set_binary_property_value(signature, "signature", sign, (uint32_t) signlen);
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
            &dh_modp_pub_key, challengeData, challengeDataSize, NULL, NULL, 0, NULL, NULL, HANDSHAKE_REQUEST);
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
    const DDS_Security_OctetSeq *challenge1,
    const DDS_Security_OctetSeq *challenge2,
    HandshakeStep_t token_type)
{
    const DDS_Security_BinaryProperty_t *property;
    const char * class_id;

    switch (token_type)
    {
    case HANDSHAKE_REQUEST:
        class_id = AUTH_HANDSHAKE_REQUEST_TOKEN_CLASS_ID;
        break;
    case HANDSHAKE_REPLY:
        class_id = AUTH_HANDSHAKE_REPLY_TOKEN_CLASS_ID;
        break;
    case HANDSHAKE_FINAL:
        class_id = AUTH_HANDSHAKE_FINAL_TOKEN_CLASS_ID;
        break;
    default:
        class_id = NULL;
        CU_FAIL("HandshakeMessageToken invalid token type");
    }

    if (!token->class_id || strcmp(token->class_id, class_id) != 0) {
        CU_FAIL("HandshakeMessageToken incorrect class_id");
    } else if (find_binary_property(token, "hash_c2") == NULL) {
         CU_FAIL("HandshakeMessageToken incorrect property 'hash_c2' not found");
    } else if (find_binary_property(token, "dh2") == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property 'dh2' not found");
    } else if (find_binary_property(token, "hash_c1") == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property 'hash_c1' not found");
    } else if (find_binary_property(token, "dh1") == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property 'dh1' not found");
    } else if ((property = find_binary_property(token, "challenge1")) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property 'challenge1' not found");
    } else if (challenge1 && compare_octet_seq(challenge1, &property->value) != 0) {
        CU_FAIL("HandshakeMessageToken incorrect property 'challenge1' incorrect value");
    } else if ((property = find_binary_property(token, "challenge2")) == NULL) {
        CU_FAIL("HandshakeMessageToken incorrect property 'challenge2' not found");
    } else if (challenge2 && compare_octet_seq(challenge2, &property->value) != 0) {
        CU_FAIL("HandshakeMessageToken incorrect property 'challenge2' incorrect value");
    } else {
        return true;
    }

    return false;
}

CU_Test(ddssec_builtin_process_handshake,happy_day_after_request )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *dh1;
    const DDS_Security_BinaryProperty_t *challenge1_glb;
    struct octet_seq dh1_pub_key;

    CU_ASSERT_FATAL (auth != NULL);
    assert(auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert(auth->begin_handshake_request != 0);
    CU_ASSERT_FATAL (auth->process_handshake != NULL);
    assert(auth->process_handshake != 0);

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
    challenge1_glb = find_binary_property(&handshake_token_out, "challenge1");

    /*Get DH1 value */
    dh1 = find_binary_property(&handshake_token_out, "dh1");

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");

    CU_ASSERT_FATAL(dh1 != NULL);
    assert(dh1 != NULL); // for Clang's static analyzer
    CU_ASSERT_FATAL(dh1->value._length > 0);
    CU_ASSERT_FATAL(dh1->value._buffer != NULL);
    assert(dh1->value._length > 0 && dh1->value._buffer != NULL); // for Clang's static analyzer

    dh1_pub_key.data = dh1->value._buffer;
    dh1_pub_key.length = dh1->value._length;

    /* prepare reply */
    fill_handshake_message_token(
        &handshake_reply_token_in, remote_participant_data2, remote_identity_certificate,
        AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_ECDH_NAME,
        &dh1_pub_key, challenge1_glb->value._buffer, challenge1_glb->value._length,
        &dh_ecdh_pub_key, challenge2_predefined_glb->value._buffer, challenge2_predefined_glb->value._length, hash1_sentrequest, NULL, HANDSHAKE_REPLY);

    reset_exception(&exception);

    result = auth->process_handshake(
                        auth,
                        &handshake_reply_token_out,
                        &handshake_reply_token_in,
                        handshake_handle,
                        &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE);
    CU_ASSERT(handshake_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT(validate_handshake_token(&handshake_reply_token_out, &challenge1_glb->value, &challenge2_predefined_glb->value, HANDSHAKE_FINAL));

    CU_ASSERT( check_shared_secret(auth, 1, dh1, dh_ecdh_key, handshake_handle)== 0);

    success= auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_reply_token_in);
    handshake_message_deinit(&handshake_reply_token_out);
}

CU_Test(ddssec_builtin_process_handshake,happy_day_after_reply )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_final_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_final_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *hash2_sentreply;
    const DDS_Security_BinaryProperty_t *challenge2_glb;
    const DDS_Security_BinaryProperty_t *dh2;
    struct octet_seq dh2_pub_key;

    CU_ASSERT_FATAL (auth->process_handshake != NULL);

    CU_ASSERT_FATAL (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);

    fill_handshake_message_token_default(&handshake_token_in, remote_participant_data1, challenge1_predefined_glb->value._buffer, challenge1_predefined_glb->value._length);

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
    CU_ASSERT(validate_handshake_token(&handshake_token_out, &challenge1_predefined_glb->value, NULL, HANDSHAKE_REPLY));

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");
    hash2_sentreply = find_binary_property(&handshake_token_out, "hash_c2");

    /*Get DH2 value */
    dh2 = find_binary_property(&handshake_token_out, "dh2");

    /* get challenge 2 from the message */
    challenge2_glb = find_binary_property(&handshake_token_out, "challenge2");

    reset_exception(&exception);

    /* prepare final */
    dh2_pub_key.data = dh2->value._buffer;
    dh2_pub_key.length = dh2->value._length;

    fill_handshake_message_token(
                &handshake_final_token_in, NULL, remote_identity_certificate,
                AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_ECDH_NAME,
                &dh_modp_pub_key, challenge1_predefined_glb->value._buffer, challenge1_predefined_glb->value._length,
                &dh2_pub_key, challenge2_glb->value._buffer, challenge2_glb->value._length, hash1_sentrequest, hash2_sentreply, HANDSHAKE_FINAL);

    result = auth->process_handshake(
                        auth,
                        &handshake_final_token_out,
                        &handshake_final_token_in,
                        handshake_handle,
                        &exception);


    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_OK);

    CU_ASSERT( check_shared_secret(auth, 0, dh2, dh_modp_key, handshake_handle)== 0);

    success= auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_final_token_in);
    handshake_message_deinit(&handshake_final_token_out);
}

CU_Test(ddssec_builtin_process_handshake,invalid_arguments )
{

    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_final_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *hash2_sentreply;
    const DDS_Security_BinaryProperty_t *challenge2_glb;
    const DDS_Security_BinaryProperty_t *dh2;
    struct octet_seq dh2_pub_key;
    DDS_Security_boolean success;

    CU_ASSERT_FATAL (auth->process_handshake != NULL);

    CU_ASSERT_FATAL (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);

    fill_handshake_message_token_default(&handshake_token_in, remote_participant_data1, challenge1_predefined_glb->value._buffer, challenge1_predefined_glb->value._length);

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
    CU_ASSERT(validate_handshake_token(&handshake_token_out, &challenge1_predefined_glb->value, NULL, HANDSHAKE_REPLY));

    /*Get DH2 value */
    dh2 = find_binary_property(&handshake_token_out, "dh2");

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");
    hash2_sentreply = find_binary_property(&handshake_token_out, "hash_c2");

    /* get challenge 2 from the message */
    challenge2_glb = find_binary_property(&handshake_token_out, "challenge2");

    reset_exception(&exception);

    /* prepare final */
    dh2_pub_key.data = dh2->value._buffer;
    dh2_pub_key.length = dh2->value._length;

    fill_handshake_message_token(
                &handshake_final_token_in, NULL, remote_identity_certificate,
                AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
                &dh_modp_pub_key, challenge1_predefined_glb->value._buffer, challenge1_predefined_glb->value._length,
                &dh2_pub_key, challenge2_glb->value._buffer, challenge2_glb->value._length,
                hash1_sentrequest, hash2_sentreply, HANDSHAKE_FINAL);


    result = auth->process_handshake(
                        auth,
                        NULL,
                        &handshake_final_token_in,
                        handshake_handle,
                        &exception);

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->process_handshake(
                        auth,
                        &handshake_token_out,
                        NULL,
                        handshake_handle,
                        &exception);

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    result = auth->process_handshake(
                        auth,
                        &handshake_token_out,
                        &handshake_final_token_in,
                        0,
                        &exception);

    CU_ASSERT (result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);
    reset_exception(&exception);

    success= auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_final_token_in);
}


CU_Test(ddssec_builtin_process_handshake,invalid_certificate )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *challenge1_glb;
    const DDS_Security_BinaryProperty_t *dh1;
    struct octet_seq dh1_pub_key;
    DDS_Security_boolean success;

    CU_ASSERT_FATAL (auth != NULL);
    assert(auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert(auth->begin_handshake_request != 0);
    CU_ASSERT_FATAL (auth->process_handshake != NULL);
    assert(auth->process_handshake != 0);

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

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);

    /* get challenge 1 from the message */
    challenge1_glb = find_binary_property(&handshake_token_out, "challenge1");

    /*Get DH1 value */
    dh1 = find_binary_property(&handshake_token_out, "dh1");

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");

    CU_ASSERT_FATAL(dh1 != NULL);
    assert(dh1 != NULL); // for Clang's static analyzer
    CU_ASSERT_FATAL(dh1->value._length > 0);
    CU_ASSERT_FATAL(dh1->value._buffer != NULL);
    assert(dh1->value._length > 0 && dh1->value._buffer != NULL); // for Clang's static analyzer

    /* prepare reply */
    dh1_pub_key.data = dh1->value._buffer;
    dh1_pub_key.length = dh1->value._length;

    fill_handshake_message_token(
       &handshake_reply_token_in, remote_participant_data2, unrelated_identity,
       AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_ECDH_NAME,
       &dh1_pub_key, challenge1_glb->value._buffer, challenge1_glb->value._length,
       &dh_ecdh_pub_key, challenge2_predefined_glb->value._buffer, challenge2_predefined_glb->value._length,
       hash1_sentrequest, NULL, HANDSHAKE_REPLY);

    reset_exception(&exception);

    result = auth->process_handshake(
                       auth,
                       &handshake_reply_token_out,
                       &handshake_reply_token_in,
                       handshake_handle,
                       &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);

    success= auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_reply_token_in);
}

CU_Test(ddssec_builtin_process_handshake,invalid_dsign_algo )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *challenge1_glb;
    const DDS_Security_BinaryProperty_t *dh1;
    struct octet_seq dh1_pub_key;

    CU_ASSERT_FATAL (auth != NULL);
    assert(auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert(auth->begin_handshake_request != 0);
    CU_ASSERT_FATAL (auth->process_handshake != NULL);
    assert(auth->process_handshake != 0);

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

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);

    /* get challenge 1 from the message */
    challenge1_glb = find_binary_property(&handshake_token_out, "challenge1");

    /*Get DH1 value */
    dh1 = find_binary_property(&handshake_token_out, "dh1");

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");

    CU_ASSERT_FATAL(dh1 != NULL);
    assert(dh1 != NULL); // for Clang's static analyzer
    CU_ASSERT_FATAL(dh1->value._length > 0);
    CU_ASSERT_FATAL(dh1->value._buffer != NULL);
    assert(dh1->value._length > 0 && dh1->value._buffer != NULL); // for Clang's static analyzer

    /* prepare reply */
    dh1_pub_key.data = dh1->value._buffer;
    dh1_pub_key.length = dh1->value._length;

    fill_handshake_message_token(
       &handshake_reply_token_in, remote_participant_data2, remote_identity_certificate,
       "RSASSA-PSS-SHA128", AUTH_KAGREE_ALGO_RSA_NAME,
       &dh1_pub_key, challenge1_glb->value._buffer, challenge1_glb->value._length,
       &dh_modp_pub_key, challenge2_predefined_glb->value._buffer, challenge2_predefined_glb->value._length,
       hash1_sentrequest, NULL, HANDSHAKE_REPLY);

    reset_exception(&exception);

    result = auth->process_handshake(
                       auth,
                       &handshake_reply_token_out,
                       &handshake_reply_token_in,
                       handshake_handle,
                       &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);
    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_reply_token_in);
}

CU_Test(ddssec_builtin_process_handshake,invalid_kagree_algo )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *challenge1_glb;
    const DDS_Security_BinaryProperty_t *dh1;
    struct octet_seq dh1_pub_key;

    CU_ASSERT_FATAL (auth != NULL);
    assert(auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert(auth->begin_handshake_request != 0);
    CU_ASSERT_FATAL (auth->process_handshake != NULL);
    assert(auth->process_handshake != 0);

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

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);

    /* get challenge 1 from the message */
    challenge1_glb = find_binary_property(&handshake_token_out, "challenge1");

    /*Get DH1 value */
    dh1 = find_binary_property(&handshake_token_out, "dh1");

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");

    CU_ASSERT_FATAL(dh1 != NULL);
    assert (dh1 != NULL); // for Clang's static analyzer
    CU_ASSERT_FATAL(dh1->value._length > 0);
    CU_ASSERT_FATAL(dh1->value._buffer != NULL);
    assert (dh1->value._length > 0 && dh1->value._buffer != NULL); // for Clang's static analyzer

    /* prepare reply */
    dh1_pub_key.data = dh1->value._buffer;
    dh1_pub_key.length = dh1->value._length;

    fill_handshake_message_token(
       &handshake_reply_token_in, remote_participant_data2, remote_identity_certificate,
       AUTH_DSIGN_ALGO_RSA_NAME, "DH+MODP-2048-128",
       &dh1_pub_key, challenge1_glb->value._buffer, challenge1_glb->value._length,
       &dh_modp_pub_key,  challenge2_predefined_glb->value._buffer, challenge2_predefined_glb->value._length,
       hash1_sentrequest, NULL, HANDSHAKE_REPLY);

    reset_exception(&exception);

    result = auth->process_handshake(
                       auth,
                       &handshake_reply_token_out,
                       &handshake_reply_token_in,
                       handshake_handle,
                       &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);
    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_reply_token_in);
}

CU_Test(ddssec_builtin_process_handshake,invalid_diffie_hellman )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *challenge1_glb;
    const DDS_Security_BinaryProperty_t *dh1;

    CU_ASSERT_FATAL (auth != NULL);
    assert(auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert(auth->begin_handshake_request != 0);
    CU_ASSERT_FATAL (auth->process_handshake != NULL);
    assert(auth->process_handshake != 0);

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

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);

    /* get challenge 1 from the message */
    challenge1_glb = find_binary_property(&handshake_token_out, "challenge1");

    /*Get DH1 value */
    dh1 = find_binary_property(&handshake_token_out, "dh1");

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");

    CU_ASSERT_FATAL(dh1 != NULL);
    assert (dh1 != NULL); // for Clang's static analyzer
    CU_ASSERT_FATAL(dh1->value._length > 0);
    CU_ASSERT_FATAL(dh1->value._buffer != NULL);
    assert (dh1->value._length > 0 && dh1->value._buffer != NULL); // for Clang's static analyzer

    /* prepare reply */
    fill_handshake_message_token(
       &handshake_reply_token_in, remote_participant_data2, remote_identity_certificate,
       AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
       &invalid_dh_pub_key, challenge1_glb->value._buffer, challenge1_glb->value._length,
       &dh_modp_pub_key, challenge2_predefined_glb->value._buffer, challenge2_predefined_glb->value._length,
       hash1_sentrequest, NULL, HANDSHAKE_REPLY);

    reset_exception(&exception);

    result = auth->process_handshake(
                       auth,
                       &handshake_reply_token_out,
                       &handshake_reply_token_in,
                       handshake_handle,
                       &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT (exception.code != 0);
    CU_ASSERT (exception.message != NULL);

    reset_exception(&exception);
    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_reply_token_in);
}


CU_Test(ddssec_builtin_process_handshake,return_handle)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    CU_ASSERT_FATAL (auth != NULL);
    assert(auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_reply != NULL);
    assert(auth->begin_handshake_reply != 0);

    fill_handshake_message_token_default(&handshake_token_in, remote_participant_data1, challenge1_predefined_glb->value._buffer, challenge1_predefined_glb->value._length);

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
        printf("begin_handshake_request failed: %s\n", exception.message ? exception.message : "Error message missing");
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
    CU_ASSERT_FALSE (success);
    CU_ASSERT (exception.minor_code != 0);
    CU_ASSERT (exception.message != NULL);

    if (!success) {
        printf("return_handshake_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
}


CU_Test(ddssec_builtin_process_handshake,extended_certificate_check )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_reply_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *dh1;
    const DDS_Security_BinaryProperty_t *challenge1_glb;
    struct octet_seq dh1_pub_key;

    release_local_identity();
    release_remote_identities();

    CU_ASSERT_FATAL( !validate_local_identity("trusted_ca_dir") );
    CU_ASSERT_FATAL( !validate_remote_identities( remote_identity_trusted ) );

    CU_ASSERT_FATAL (auth != NULL);
    assert(auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert(auth->begin_handshake_request != 0);
    CU_ASSERT_FATAL (auth->process_handshake != NULL);
    assert(auth->process_handshake != 0);

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
    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    assert(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE); // for Clang's static analyzer

    /* get challenge 1 from the message */
    challenge1_glb = find_binary_property(&handshake_token_out, "challenge1");

    /*Get DH1 value */
    dh1 = find_binary_property(&handshake_token_out, "dh1");

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");

    CU_ASSERT_FATAL(dh1 != NULL);
    assert(dh1 != NULL); // for Clang's static analyzer
    CU_ASSERT_FATAL(dh1->value._length > 0);
    CU_ASSERT_FATAL(dh1->value._buffer != NULL);
    assert(dh1->value._length > 0 && dh1->value._buffer != NULL); // for Clang's static analyzer

    dh1_pub_key.data = dh1->value._buffer;
    dh1_pub_key.length = dh1->value._length;

    /* prepare reply */
    fill_handshake_message_token(
        &handshake_reply_token_in, remote_participant_data2, remote_identity_trusted,
        AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_ECDH_NAME,
        &dh1_pub_key, challenge1_glb->value._buffer, challenge1_glb->value._length,
        &dh_ecdh_pub_key, challenge2_predefined_glb->value._buffer, challenge2_predefined_glb->value._length, hash1_sentrequest, NULL, HANDSHAKE_REPLY);

    reset_exception(&exception);

    result = auth->process_handshake(
                        auth,
                        &handshake_reply_token_out,
                        &handshake_reply_token_in,
                        handshake_handle,
                        &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE);
    CU_ASSERT(handshake_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT(validate_handshake_token(&handshake_reply_token_out, &challenge1_glb->value, &challenge2_predefined_glb->value, HANDSHAKE_FINAL));

    CU_ASSERT( check_shared_secret(auth, 1, dh1, dh_ecdh_key, handshake_handle)== 0);

    success= auth->return_handshake_handle(auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_reply_token_in);
    handshake_message_deinit(&handshake_reply_token_out);


    release_local_identity();
    release_remote_identities();

    CU_ASSERT_FATAL( !validate_local_identity("trusted_ca_dir") );
    CU_ASSERT_FATAL( !validate_remote_identities( remote_identity_trusted_expired ) );

    CU_ASSERT_FATAL (auth != NULL);
    assert(auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert(auth->begin_handshake_request != 0);
    CU_ASSERT_FATAL (auth->process_handshake != NULL);
    assert(auth->process_handshake != 0);

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
    challenge1_glb = find_binary_property(&handshake_token_out, "challenge1");

    /*Get DH1 value */
    dh1 = find_binary_property(&handshake_token_out, "dh1");

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");

    CU_ASSERT_FATAL(dh1 != NULL);
    assert (dh1 != NULL); // for Clang's static analyzer
    CU_ASSERT_FATAL(dh1->value._length > 0);
    CU_ASSERT_FATAL(dh1->value._buffer != NULL);
    assert (dh1->value._length > 0 && dh1->value._buffer != NULL); // for Clang's static analyzer

    dh1_pub_key.data = dh1->value._buffer;
    dh1_pub_key.length = dh1->value._length;

    /* prepare reply */
    fill_handshake_message_token(
        &handshake_reply_token_in, remote_participant_data2, remote_identity_trusted_expired,
        AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_ECDH_NAME,
        &dh1_pub_key, challenge1_glb->value._buffer, challenge1_glb->value._length,
        &dh_ecdh_pub_key, challenge2_predefined_glb->value._buffer, challenge2_predefined_glb->value._length, hash1_sentrequest, NULL, HANDSHAKE_REPLY);

    reset_exception(&exception);

    result = auth->process_handshake(
                        auth,
                        &handshake_reply_token_out,
                        &handshake_reply_token_in,
                        handshake_handle,
                        &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_FAILED);

    reset_exception(&exception);


    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_reply_token_in);
    handshake_message_deinit(&handshake_reply_token_out);



    release_local_identity();
    release_remote_identities();

    CU_ASSERT_FATAL( !validate_local_identity("trusted_ca_dir") );
    CU_ASSERT_FATAL( !validate_remote_identities( remote_identity_untrusted ) );

    CU_ASSERT_FATAL (auth != NULL);
    assert (auth != NULL);
    CU_ASSERT_FATAL (local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle1 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (remote_identity_handle2 != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (auth->begin_handshake_request != NULL);
    assert (auth->begin_handshake_request != 0);
    CU_ASSERT_FATAL (auth->process_handshake != NULL);
    assert (auth->process_handshake != 0);

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
    challenge1_glb = find_binary_property(&handshake_token_out, "challenge1");

    /*Get DH1 value */
    dh1 = find_binary_property(&handshake_token_out, "dh1");

    hash1_sentrequest = find_binary_property(&handshake_token_out, "hash_c1");

    CU_ASSERT_FATAL(dh1 != NULL);
    assert (dh1 != NULL); // for Clang's static analyzer
    CU_ASSERT_FATAL(dh1->value._length > 0);
    CU_ASSERT_FATAL(dh1->value._buffer != NULL);
    assert (dh1->value._length > 0 && dh1->value._buffer != NULL); // for Clang's static analyzer

    dh1_pub_key.data = dh1->value._buffer;
    dh1_pub_key.length = dh1->value._length;

    /* prepare reply */
    fill_handshake_message_token(
        &handshake_reply_token_in, remote_participant_data2, remote_identity_untrusted,
        AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_ECDH_NAME,
        &dh1_pub_key, challenge1_glb->value._buffer, challenge1_glb->value._length,
        &dh_ecdh_pub_key, challenge2_predefined_glb->value._buffer, challenge2_predefined_glb->value._length, hash1_sentrequest, NULL, HANDSHAKE_REPLY);

    reset_exception(&exception);

    result = auth->process_handshake(
                        auth,
                        &handshake_reply_token_out,
                        &handshake_reply_token_in,
                        handshake_handle,
                        &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_FAILED);
    CU_ASSERT_FATAL(exception.code != 0);


    reset_exception(&exception);


    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_reply_token_in);
    handshake_message_deinit(&handshake_reply_token_out);

}
