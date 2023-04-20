// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/misc.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/openssl_support.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "common/src/loader.h"
#include "common/src/handshake_helper.h"
#include "auth_tokens.h"
#include "ac_tokens.h"

#define HANDSHAKE_SIGNATURE_SIZE 6

static const char * SUBJECT_NAME_IDENTITY_CERT      = "CN=CHAM-574 client,O=Some Company,ST=Some-State,C=NL";
static const char * SUBJECT_NAME_IDENTITY_CA        = "CN=CHAM-574 authority,O=Some Company,ST=Some-State,C=NL";

static const char * RSA_2048_ALGORITHM_NAME         = "RSA-2048";

static const char * AUTH_HANDSHAKE_REQUEST_TOKEN_CLASS_ID = "DDS:Auth:PKI-DH:1.0+Req";
static const char * AUTH_HANDSHAKE_REPLY_TOKEN_CLASS_ID   = "DDS:Auth:PKI-DH:1.0+Reply";
static const char * AUTH_HANDSHAKE_FINAL_TOKEN_CLASS_ID   = "DDS:Auth:PKI-DH:1.0+Final";

static const char * PERMISSIONS_DOCUMENT                  = "permissions_document";

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

static const char *REMOTE_IDENTITY_CERTIFICATE =
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



static struct plugins_hdl *g_plugins = NULL;
static dds_security_authentication *g_auth = NULL;

static DDS_Security_IdentityHandle g_local_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_IdentityHandle g_remote_identity_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_AuthRequestMessageToken g_remote_auth_request_token = DDS_SECURITY_TOKEN_INIT;
static const DDS_Security_BinaryProperty_t *g_challenge1_predefined_glb = NULL;
static const DDS_Security_BinaryProperty_t *g_challenge2_predefined_glb = NULL;
static DDS_Security_OctetSeq g_serialized_participant_data = DDS_SECURITY_SEQUENCE_INIT;
static DDS_Security_ParticipantBuiltinTopicData *g_local_participant_data = NULL;

static DDS_Security_ParticipantBuiltinTopicData *g_remote_participant_data1 = NULL;
static DDS_Security_ParticipantBuiltinTopicData *g_remote_participant_data2 = NULL;
static DDS_Security_GUID_t g_candidate_participant_guid;
static DDS_Security_GUID_t g_remote_participant_guid1;
static DDS_Security_GUID_t g_remote_participant_guid2;

static EVP_PKEY *g_dh_modp_key = NULL;
static EVP_PKEY *g_dh_ecdh_key = NULL;
static struct octet_seq g_dh_modp_pub_key = {NULL, 0};
static struct octet_seq g_dh_ecdh_pub_key = {NULL, 0};


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
    size_t len = 32;
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

    token->binary_properties._buffer->value._maximum = (DDS_Security_unsigned_long) len;
    token->binary_properties._buffer->value._length = (DDS_Security_unsigned_long) len;
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


static DDS_Security_Property_t *
find_property(
    DDS_Security_DataHolder *token,
    const char *name)
{
    DDS_Security_Property_t *result = NULL;
    uint32_t i;

    for (i = 0; i < token->properties._length && !result; i++) {
        if (token->properties._buffer[i].name && (strcmp(token->properties._buffer[i].name, name) == 0)) {
            result = &token->properties._buffer[i];
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
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_GUID_t local_participant_guid;
    DDS_Security_GuidPrefix_t prefix = {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb};
    DDS_Security_EntityId_t entityId = {{0xb0,0xb1,0xb2},0x1};
    unsigned char *sdata;
    size_t sz;

    memset(&local_participant_guid, 0, sizeof(local_participant_guid));
    memcpy(&g_candidate_participant_guid.prefix, &prefix, sizeof(prefix));
    memcpy(&g_candidate_participant_guid.entityId, &entityId, sizeof(entityId));

    memset(&participant_qos, 0, sizeof(participant_qos));
    dds_security_property_init(&participant_qos.property.value, 3);
    participant_qos.property.value._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
    participant_qos.property.value._buffer[0].value = ddsrt_strdup(identity_certificate);
    participant_qos.property.value._buffer[1].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
    participant_qos.property.value._buffer[1].value = ddsrt_strdup(identity_ca);
    participant_qos.property.value._buffer[2].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
    participant_qos.property.value._buffer[2].value = ddsrt_strdup(private_key);

    /* Now call the function. */
    result = g_auth->validate_local_identity(
                            g_auth,
                            &g_local_identity_handle,
                            &local_participant_guid,
                            domain_id,
                            &participant_qos,
                            &g_candidate_participant_guid,
                            &exception);

    if (result != DDS_SECURITY_VALIDATION_OK) {
        res = -1;
        printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    dds_security_property_deinit(&participant_qos.property.value);
    reset_exception(&exception);

    g_local_participant_data = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&g_local_participant_data->key[0], &local_participant_guid, 12);
    /* convert from big-endian format to native format */
    g_local_participant_data->key[0] = ddsrt_fromBE4u(g_local_participant_data->key[0]);
    g_local_participant_data->key[1] = ddsrt_fromBE4u(g_local_participant_data->key[1]);
    g_local_participant_data->key[2] = ddsrt_fromBE4u(g_local_participant_data->key[2]);

    initialize_identity_token(&g_local_participant_data->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    initialize_permissions_token(&g_local_participant_data->permissions_token, RSA_2048_ALGORITHM_NAME);

    g_local_participant_data->security_info.participant_security_attributes = 0x01;
    g_local_participant_data->security_info.plugin_participant_security_attributes = 0x02;

    serializer_participant_data(g_local_participant_data, &sdata, &sz);

    g_serialized_participant_data._length = g_serialized_participant_data._maximum = (DDS_Security_unsigned_long) sz;
    g_serialized_participant_data._buffer = sdata;

    return res;
}

static void
release_local_identity(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    if (g_local_identity_handle != DDS_SECURITY_HANDLE_NIL) {
        success = g_auth->return_identity_handle(g_auth, g_local_identity_handle, &exception);
        if (!success) {
            printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
        }
        reset_exception(&exception);
    }

    DDS_Security_OctetSeq_deinit(&g_serialized_participant_data);

    if (g_local_participant_data) {
       DDS_Security_ParticipantBuiltinTopicData_free(g_local_participant_data);
    }
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
    int32_t i, size;

    name = X509_get_subject_name(cert);
    size = i2d_X509_NAME(name, &tmp);
    if (size > 0) {
        subject = ddsrt_malloc((size_t) size);
        memcpy(subject, tmp, (size_t)size);
        OPENSSL_free(tmp);

        SHA256(subject, (size_t)size, high);
        SHA256(&candidate->prefix[0], sizeof(DDS_Security_GuidPrefix_t), low);

        adjusted->entityId = candidate->entityId;
        for (i = 0; i < 6; i++) {
            adjusted->prefix[i] = hb | high[i]>>1;
            hb = (unsigned char)( high[i]<<7 );
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

    if (g_local_identity_handle == DDS_SECURITY_HANDLE_NIL) {
        return -1;
    }

    cert = load_certificate(remote_id_certificate);
    if (!cert) {
        return -1;
    }

    if (!get_adjusted_participant_guid(cert, &guid1, &g_remote_participant_guid1)) {
        X509_free(cert);
        return -1;
    }

    if (!get_adjusted_participant_guid(cert, &guid2, &g_remote_participant_guid2)) {
        X509_free(cert);
        return -1;
    }

    X509_free(cert);

    initialize_identity_token(&remote_identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);

    reset_exception(&exception);

    fill_auth_request_token(&g_remote_auth_request_token);

    result = g_auth->validate_remote_identity(
                g_auth,
                &g_remote_identity_handle,
                &local_auth_request_token,
                &g_remote_auth_request_token,
                g_local_identity_handle,
                &remote_identity_token,
                &g_remote_participant_guid2,
                &exception);

    if ((result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST) &&
            (result != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)) {
        printf("validate_remote_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
    }

    reset_exception(&exception);

    deinitialize_identity_token(&remote_identity_token);
    DDS_Security_DataHolder_deinit(&local_auth_request_token);

    g_remote_participant_data1 = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&g_remote_participant_data1->key[0], &g_remote_participant_guid1, 12);
    g_remote_participant_data1->key[0] = ddsrt_fromBE4u(g_remote_participant_data1->key[0]);
    g_remote_participant_data1->key[1] = ddsrt_fromBE4u(g_remote_participant_data1->key[1]);
    g_remote_participant_data1->key[2] = ddsrt_fromBE4u(g_remote_participant_data1->key[2]);

    initialize_identity_token(&g_remote_participant_data1->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    initialize_permissions_token(&g_remote_participant_data1->permissions_token, RSA_2048_ALGORITHM_NAME);

    g_remote_participant_data1->security_info.participant_security_attributes = 0x01;
    g_remote_participant_data1->security_info.plugin_participant_security_attributes = 0x02;

    g_remote_participant_data2 = DDS_Security_ParticipantBuiltinTopicData_alloc();
    memcpy(&g_remote_participant_data2->key[0], &g_remote_participant_guid2, 12);
    g_remote_participant_data2->key[0] = ddsrt_fromBE4u(g_remote_participant_data2->key[0]);
    g_remote_participant_data2->key[1] = ddsrt_fromBE4u(g_remote_participant_data2->key[1]);
    g_remote_participant_data2->key[2] = ddsrt_fromBE4u(g_remote_participant_data2->key[2]);

    initialize_identity_token(&g_remote_participant_data2->identity_token, RSA_2048_ALGORITHM_NAME, RSA_2048_ALGORITHM_NAME);
    initialize_permissions_token(&g_remote_participant_data2->permissions_token, RSA_2048_ALGORITHM_NAME);

    g_remote_participant_data2->security_info.participant_security_attributes = 0x01;
    g_remote_participant_data2->security_info.plugin_participant_security_attributes = 0x02;

    g_remote_participant_data2->security_info.participant_security_attributes = 0x01;
    g_remote_participant_data2->security_info.plugin_participant_security_attributes = 0x02;

    g_challenge1_predefined_glb = find_binary_property(&g_remote_auth_request_token, DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE);
    g_challenge2_predefined_glb = g_challenge1_predefined_glb;

    return res;
}

static void
release_remote_identities(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;

    if (g_remote_identity_handle != DDS_SECURITY_HANDLE_NIL) {
        success = g_auth->return_identity_handle(g_auth, g_remote_identity_handle, &exception);
        if (!success) {
            printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
        }
        reset_exception(&exception);
    }

    DDS_Security_DataHolder_deinit(&g_remote_auth_request_token);

    DDS_Security_ParticipantBuiltinTopicData_free(g_remote_participant_data1);
    DDS_Security_ParticipantBuiltinTopicData_free(g_remote_participant_data2);
}

CU_Init(ddssec_builtin_get_authenticated_peer_credential)
{
    int result = 0;
    dds_openssl_init ();

    /* Only need the authentication plugin. */
    g_plugins = load_plugins(NULL   /* Access Control */,
                             &g_auth  /* Authentication */,
                             NULL   /* Cryptograpy    */,
                             &(const struct ddsi_domaingv){ .handshake_include_optional = true });
    if (g_plugins) {
        result = validate_local_identity();
        if (result >= 0) {
            result = validate_remote_identities( REMOTE_IDENTITY_CERTIFICATE );
        }
        if (result >= 0) {
            result = create_dh_key_modp_2048(&g_dh_modp_key);
        }
        if (result >= 0) {
            result = get_dh_public_key_modp_2048(g_dh_modp_key, &g_dh_modp_pub_key);
        }
        if (result >= 0) {
            result = create_dh_key_ecdh(&g_dh_ecdh_key);
        }
        if (result >= 0) {
            result = get_dh_public_key_ecdh(g_dh_ecdh_key, &g_dh_ecdh_pub_key);
        }
    } else {
        result = -1;
    }


    return result;
}

CU_Clean(ddssec_builtin_get_authenticated_peer_credential)
{
    release_local_identity();
    release_remote_identities();
    unload_plugins(g_plugins);
    octet_seq_deinit(&g_dh_modp_pub_key);
    octet_seq_deinit(&g_dh_ecdh_pub_key);
    if (g_dh_modp_key) {
        EVP_PKEY_free(g_dh_modp_key);
    }
    if (g_dh_ecdh_key) {
        EVP_PKEY_free(g_dh_ecdh_key);
    }
    return 0;
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
    DDS_Security_Serializer_buffer(serializer, buffer,  size);
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
           set_binary_property_string(c_id, DDS_AUTHTOKEN_PROP_C_ID, certificate);
       } else {
           set_binary_property_string(c_id, DDS_AUTHTOKEN_PROP_C_ID "x", "rubbish");
       }

       /* Store the permission document in the c.perm property */
       set_binary_property_string(c_perm, DDS_AUTHTOKEN_PROP_C_PERM, PERMISSIONS_DOCUMENT);

       /* Store the provided g_local_participant_data in the c.pdata property */
       set_binary_property_value(c_pdata, DDS_AUTHTOKEN_PROP_C_PDATA, serialized_local_participant_data, serialized_local_participant_data_size);
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

           set_binary_property_value(hash_c1, DDS_AUTHTOKEN_PROP_HASH_C1, hash1_sentrequest_arr, sizeof(hash1_sentrequest_arr));
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
           set_binary_property_value(challenge1, DDS_AUTHTOKEN_PROP_CHALLENGE1 "x", g_challenge1_predefined_glb->value._buffer, g_challenge1_predefined_glb->value._length);
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
        set_binary_property_string(c_perm, DDS_AUTHTOKEN_PROP_C_PERM, PERMISSIONS_DOCUMENT);

        /* Store the provided g_local_participant_data in the c.pdata property */
        set_binary_property_value(c_pdata, DDS_AUTHTOKEN_PROP_C_PDATA, serialized_local_participant_data, serialized_local_participant_data_size);
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

        CU_ASSERT(hash1_from_request != NULL);

        set_binary_property_value(hash_c1, DDS_AUTHTOKEN_PROP_HASH_C1, hash1_from_request->value._buffer, hash1_from_request->value._length);

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

           set_binary_property_value(hash_c2, DDS_AUTHTOKEN_PROP_HASH_C2, hash2_sentreply_arr, sizeof(hash2_sentreply_arr));
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
            set_binary_property_value(signature, DDS_AUTHTOKEN_PROP_SIGNATURE, sign, signlen);

            ddsrt_free(sign);
            BIO_free(bio);
            EVP_PKEY_free(private_key_x509);
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
        assert(hash1_from_request && hash2_from_reply); // for Clang's static analyzer

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
            set_binary_property_value(signature, DDS_AUTHTOKEN_PROP_SIGNATURE, sign, signlen);

            ddsrt_free(sign);
            BIO_free(bio);
            EVP_PKEY_free(private_key_x509);
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
            token, pdata, REMOTE_IDENTITY_CERTIFICATE,
            AUTH_DSIGN_ALGO_RSA_NAME, AUTH_KAGREE_ALGO_RSA_NAME,
            &g_dh_modp_pub_key, challengeData, challengeDataSize, NULL, NULL, 0, NULL, NULL, HANDSHAKE_REQUEST);
}

static void
handshake_message_deinit(
    DDS_Security_HandshakeMessageToken *token)
{
    DDS_Security_DataHolder_deinit(token);
}

CU_Test(ddssec_builtin_get_authenticated_peer_credential,token_after_request )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_reply_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_AuthenticatedPeerCredentialToken credential_token = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_boolean success;
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *dh1;
    const DDS_Security_BinaryProperty_t *challenge1_glb;
    const DDS_Security_Property_t *c_id;
    const DDS_Security_Property_t *c_perm;
    struct octet_seq dh1_pub_key;

    CU_ASSERT_FATAL (g_auth != NULL);
    CU_ASSERT_FATAL (g_local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (g_remote_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (g_auth->begin_handshake_request != NULL);
    CU_ASSERT_FATAL (g_auth->process_handshake != NULL);

    /* simulate request */
    result = g_auth->begin_handshake_request(
                    g_auth,
                    &handshake_handle,
                    &handshake_token_out,
                    g_local_identity_handle,
                    g_remote_identity_handle,
                    &g_serialized_participant_data,
                    &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    assert(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE); // for Clang's static analyzer

    /* mock reply */
    dh1 = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_DH1);
    dh1_pub_key.data = dh1->value._buffer;
    dh1_pub_key.length = dh1->value._length;
    challenge1_glb = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_CHALLENGE1);
    hash1_sentrequest = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_HASH_C1);
    fill_handshake_message_token(
                    &handshake_reply_token_in,
                    g_remote_participant_data2,
                    REMOTE_IDENTITY_CERTIFICATE,
                    AUTH_DSIGN_ALGO_RSA_NAME,
                    AUTH_KAGREE_ALGO_ECDH_NAME,
                    &dh1_pub_key,
                    challenge1_glb->value._buffer,
                    challenge1_glb->value._length,
                    &g_dh_ecdh_pub_key,
                    g_challenge2_predefined_glb->value._buffer,
                    g_challenge2_predefined_glb->value._length,
                    hash1_sentrequest,
                    NULL,
                    HANDSHAKE_REPLY);
    handshake_message_deinit(&handshake_token_out);

    /* simulate process */
    result = g_auth->process_handshake(
                        g_auth,
                        &handshake_token_out,
                        &handshake_reply_token_in,
                        handshake_handle,
                        &exception);
    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE);
    assert(result == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE); // for Clang's static analyzer

    /*
     * Actual test.
     */
    success = g_auth->get_authenticated_peer_credential_token(
                        g_auth,
                        &credential_token,
                        handshake_handle,
                        &exception);

    CU_ASSERT_TRUE (success);
    assert(success); // for Clang's static analyzer

    CU_ASSERT_FATAL(credential_token.class_id != NULL);
    assert(credential_token.class_id); // for Clang's static analyzer
    CU_ASSERT(strcmp(credential_token.class_id, DDS_AUTHTOKEN_CLASS_ID) == 0);
    CU_ASSERT(credential_token.properties._length == 2);
    CU_ASSERT(credential_token.binary_properties._length == 0);

    c_id = find_property(&credential_token, DDS_AUTHTOKEN_PROP_C_ID);
    CU_ASSERT_FATAL(c_id != NULL);
    assert(c_id); // for GCC's static analyzer
    CU_ASSERT_FATAL(c_id->value != NULL);
    assert(c_id && c_id->value); // for Clang's static analyzer
    //printf("c_id->value: %s\n", c_id->value);
    CU_ASSERT(strcmp(c_id->value, REMOTE_IDENTITY_CERTIFICATE) == 0);

    c_perm = find_property(&credential_token, DDS_AUTHTOKEN_PROP_C_PERM);
    CU_ASSERT_FATAL(c_perm != NULL);
    CU_ASSERT_FATAL(c_perm->value != NULL);
    assert(c_perm && c_perm->value); // for Clang's static analyzer
    //printf("c_perm->value: %s\n", c_perm->value);
    CU_ASSERT(strcmp(c_perm->value, PERMISSIONS_DOCUMENT) == 0);

    success = g_auth->return_authenticated_peer_credential_token(g_auth, &credential_token, &exception);
    CU_ASSERT_TRUE (success);
    CU_ASSERT(credential_token.class_id == NULL);
    CU_ASSERT(credential_token.properties._buffer == NULL);
    CU_ASSERT(credential_token.properties._maximum == 0);
    CU_ASSERT(credential_token.properties._length == 0);
    CU_ASSERT(credential_token.binary_properties._buffer == NULL);
    CU_ASSERT(credential_token.binary_properties._maximum == 0);
    CU_ASSERT(credential_token.binary_properties._length == 0);

    success = g_auth->return_handshake_handle(g_auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);

    reset_exception(&exception);

    handshake_message_deinit(&handshake_reply_token_in);
    handshake_message_deinit(&handshake_token_out);
}

CU_Test(ddssec_builtin_get_authenticated_peer_credential,token_after_reply )
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_HandshakeHandle handshake_handle;
    DDS_Security_HandshakeMessageToken handshake_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_final_token_in = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeMessageToken handshake_final_token_out = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_AuthenticatedPeerCredentialToken credential_token = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_boolean success;
    const DDS_Security_BinaryProperty_t *hash1_sentrequest;
    const DDS_Security_BinaryProperty_t *hash2_sentreply;
    const DDS_Security_BinaryProperty_t *challenge2_glb;
    const DDS_Security_BinaryProperty_t *dh2;
    const DDS_Security_Property_t *c_id;
    const DDS_Security_Property_t *c_perm;
    struct octet_seq dh2_pub_key;

    CU_ASSERT_FATAL (g_auth->process_handshake != NULL);

    CU_ASSERT_FATAL (g_auth != NULL);
    CU_ASSERT_FATAL (g_local_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (g_remote_identity_handle != DDS_SECURITY_HANDLE_NIL);
    CU_ASSERT_FATAL (g_auth->begin_handshake_reply != NULL);

    /* simulate reply */
    fill_handshake_message_token_default(
                    &handshake_token_in,
                    g_remote_participant_data1,
                    g_challenge1_predefined_glb->value._buffer,
                    g_challenge1_predefined_glb->value._length);

    result = g_auth->begin_handshake_reply(
                    g_auth,
                    &handshake_handle,
                    &handshake_token_out,
                    &handshake_token_in,
                    g_remote_identity_handle,
                    g_local_identity_handle,
                    &g_serialized_participant_data,
                    &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE);
    assert(result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE); // for Clang's static analyzer

    /* mock final */
    dh2 = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_DH2);
    dh2_pub_key.data = dh2->value._buffer;
    dh2_pub_key.length = dh2->value._length;
    challenge2_glb = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_CHALLENGE2);
    hash1_sentrequest = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_HASH_C1);
    hash2_sentreply = find_binary_property(&handshake_token_out, DDS_AUTHTOKEN_PROP_HASH_C2);
    fill_handshake_message_token(
                &handshake_final_token_in,
                NULL,
                REMOTE_IDENTITY_CERTIFICATE,
                AUTH_DSIGN_ALGO_RSA_NAME,
                AUTH_KAGREE_ALGO_ECDH_NAME,
                &g_dh_modp_pub_key,
                g_challenge1_predefined_glb->value._buffer,
                g_challenge1_predefined_glb->value._length,
                &dh2_pub_key,
                challenge2_glb->value._buffer,
                challenge2_glb->value._length,
                hash1_sentrequest,
                hash2_sentreply,
                HANDSHAKE_FINAL);

    /* simulate process */
    result = g_auth->process_handshake(
                        g_auth,
                        &handshake_final_token_out,
                        &handshake_final_token_in,
                        handshake_handle,
                        &exception);

    CU_ASSERT_FATAL(result == DDS_SECURITY_VALIDATION_OK);
    assert(result == DDS_SECURITY_VALIDATION_OK); // for Clang's static analyzer

    /*
     * Actual test.
     */
    success = g_auth->get_authenticated_peer_credential_token(
                        g_auth,
                        &credential_token,
                        handshake_handle,
                        &exception);

    CU_ASSERT_TRUE (success);
    assert(success); // for Clang's static analyzer

    CU_ASSERT_FATAL(credential_token.class_id != NULL);
    CU_ASSERT(strcmp(credential_token.class_id, DDS_AUTHTOKEN_CLASS_ID) == 0);
    CU_ASSERT(credential_token.properties._length == 2);
    CU_ASSERT(credential_token.binary_properties._length == 0);

    c_id = find_property(&credential_token, DDS_AUTHTOKEN_PROP_C_ID);
    CU_ASSERT_FATAL(c_id != NULL);
    assert(c_id); // for GCC's static analyzer
    CU_ASSERT_FATAL(c_id->value != NULL);
    assert(c_id && c_id->value); // for Clang's static analyzer
    //printf("c_id->value: %s\n", c_id->value);
    CU_ASSERT(strcmp(c_id->value, REMOTE_IDENTITY_CERTIFICATE) == 0);

    c_perm = find_property(&credential_token, DDS_AUTHTOKEN_PROP_C_PERM);
    CU_ASSERT_FATAL(c_perm != NULL);
    assert(c_perm); // for Clang, GCC static analyzers
    CU_ASSERT_FATAL(c_perm->value != NULL);
    assert(c_perm->value); // for Clang's static analyzer
    //printf("c_perm->value: %s\n", c_perm->value);
    CU_ASSERT(strcmp(c_perm->value, PERMISSIONS_DOCUMENT) == 0);


    success = g_auth->return_authenticated_peer_credential_token(g_auth, &credential_token, &exception);
    CU_ASSERT_TRUE (success);
    CU_ASSERT(credential_token.class_id == NULL);
    CU_ASSERT(credential_token.properties._buffer == NULL);
    CU_ASSERT(credential_token.properties._maximum == 0);
    CU_ASSERT(credential_token.properties._length == 0);
    CU_ASSERT(credential_token.binary_properties._buffer == NULL);
    CU_ASSERT(credential_token.binary_properties._maximum == 0);
    CU_ASSERT(credential_token.binary_properties._length == 0);

    success = g_auth->return_handshake_handle(g_auth, handshake_handle, &exception);
    CU_ASSERT_TRUE (success);
    assert(success); // for Clang's static analyzer

    reset_exception(&exception);

    handshake_message_deinit(&handshake_token_in);
    handshake_message_deinit(&handshake_token_out);
    handshake_message_deinit(&handshake_final_token_in);
    handshake_message_deinit(&handshake_final_token_out);
}

CU_Test(ddssec_builtin_get_authenticated_peer_credential,token_invalid_arguments )
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_AuthenticatedPeerCredentialToken credential_token = DDS_SECURITY_TOKEN_INIT;
    DDS_Security_HandshakeHandle invalid_handle = 3;
    DDS_Security_boolean success;

    success = g_auth->get_authenticated_peer_credential_token(g_auth, &credential_token, invalid_handle, &exception);
    CU_ASSERT_FALSE (success);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);

    success = g_auth->get_authenticated_peer_credential_token(NULL, &credential_token, invalid_handle, &exception);
    CU_ASSERT_FALSE (success);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);

    success = g_auth->get_authenticated_peer_credential_token(g_auth, NULL, invalid_handle, &exception);
    CU_ASSERT_FALSE (success);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);

    success = g_auth->get_authenticated_peer_credential_token(g_auth, &credential_token, 0, &exception);
    CU_ASSERT_FALSE (success);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);

    success = g_auth->return_authenticated_peer_credential_token(NULL, &credential_token, &exception);
    CU_ASSERT_FALSE (success);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);

    success = g_auth->return_authenticated_peer_credential_token(g_auth, NULL, &exception);
    CU_ASSERT_FALSE (success);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
}
