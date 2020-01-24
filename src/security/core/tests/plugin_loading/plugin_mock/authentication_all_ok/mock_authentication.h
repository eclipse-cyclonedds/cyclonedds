/*
 * authentication.h
 *
 *  Created on: Jan 15, 2018
 *      Author: kurtulus oksuztepe
 */

#ifndef SECURITY_BUILTIN_PLUGINS_AUTHENTICATION_H_
#define SECURITY_BUILTIN_PLUGINS_AUTHENTICATION_H_

#include "dds/security/authentication_all_ok_export.h"

SECURITY_EXPORT int32_t
init_authentication(const char *argument, void **context);

SECURITY_EXPORT int32_t
finalize_authentication(void *context);

char *test_identity_certificate="data:,-----BEGIN CERTIFICATE-----\n\
MIIDYDCCAkigAwIBAgIBBDANBgkqhkiG9w0BAQsFADByMQswCQYDVQQGEwJOTDEL\n\
MAkGA1UECBMCT1YxEzARBgNVBAoTCkFETGluayBJU1QxGTAXBgNVBAMTEElkZW50\n\
aXR5IENBIFRlc3QxJjAkBgkqhkiG9w0BCQEWF2luZm9AaXN0LmFkbGlua3RlY2gu\n\
Y29tMB4XDTE4MDMxMjAwMDAwMFoXDTI3MDMxMTIzNTk1OVowdTELMAkGA1UEBhMC\n\
TkwxCzAJBgNVBAgTAk9WMRAwDgYDVQQKEwdBRExpbmsgMREwDwYDVQQLEwhJU1Qg\n\
VGVzdDETMBEGA1UEAxMKQWxpY2UgVGVzdDEfMB0GCSqGSIb3DQEJARYQYWxpY2VA\n\
YWRsaW5rLmlzdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANBW+tEZ\n\
Baw7EQCEXyzH9n7IkZ8PQIKe8hG1LAOGYOF/oUYQZJO/HxbWoC4rFqOC20+A6is6\n\
kFwr1Zzp/Wurk9CrFXo5Nomi6ActH6LUM57nYqN68w6U38z/XkQxVY/ESZ5dySfD\n\
9Q1C8R+zdE8gwbimdYmwX7ioz336nghM2CoAHPDRthQeJupl8x4V7isOltr9CGx8\n\
+imJXbGr39OK6u87cNLeu23sUkOIC0lSRMIqIQK3oJtHS70J2qecXdqp9MhE7Xky\n\
/GPlI8ptQ1gJ8A3cAOvtI9mtMJMszs2EKWTLfeTcmfJHKKhKjvCgDdh3Jan4x5YP\n\
Yg7HG6H+ceOUkMMCAwEAATANBgkqhkiG9w0BAQsFAAOCAQEAkvuqZzyJ3Nu4/Eo5\n\
kD0nVgYGBUl7cspu+636q39zPSrxLEDMUWz+u8oXLpyGcgiZ8lZulPTV8dmOn+3C\n\
Vg55c5C+gbnbX3MDyb3wB17296RmxYf6YNul4sFOmj6+g2i+Dw9WH0PBCVKbA84F\n\
jR3Gx2Pfoifor3DvT0YFSsjNIRt090u4dQglbIb6cWEafC7O24t5jFhGPvJ7L9SE\n\
gB0Drh/HmKTVuaqaRkoOKkKaKuWoXsszK1ZFda1DHommnR5LpYPsDRQ2fVM4EuBF\n\
By03727uneuG8HLuNcLEV9H0i7LxtyfFkyCPUQvWG5jehb7xPOz/Ml26NAwwjlTJ\n\
xEEFrw==\n\
-----END CERTIFICATE-----";

char *test_ca_certificate="data:,-----BEGIN CERTIFICATE-----\n\
MIIEKTCCAxGgAwIBAgIBATANBgkqhkiG9w0BAQsFADByMQswCQYDVQQGEwJOTDEL\n\
MAkGA1UECBMCT1YxEzARBgNVBAoTCkFETGluayBJU1QxGTAXBgNVBAMTEElkZW50\n\
aXR5IENBIFRlc3QxJjAkBgkqhkiG9w0BCQEWF2luZm9AaXN0LmFkbGlua3RlY2gu\n\
Y29tMB4XDTE4MDMxMjAwMDAwMFoXDTI3MDMxMTIzNTk1OVowcjELMAkGA1UEBhMC\n\
TkwxCzAJBgNVBAgTAk9WMRMwEQYDVQQKEwpBRExpbmsgSVNUMRkwFwYDVQQDExBJ\n\
ZGVudGl0eSBDQSBUZXN0MSYwJAYJKoZIhvcNAQkBFhdpbmZvQGlzdC5hZGxpbmt0\n\
ZWNoLmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANa/ENFfGVXg\n\
bPLTzBdDfiZQcp5dWZ//Pb8ErFOJu8uosVHFv8t69dgjHgNHB4OsjmjnR7GfKUZT\n\
0cMvWJnjsC7DDlBwFET9rj4k40n96bbVCH9I7+tNhsoqzc6Eu+5h4sk7VfNGTM2Z\n\
SyCd4GiSZRuA44rRbhXI7/LDpr4hY5J9ZDo5AM9ZyoLAoh774H3CZWD67S35XvUs\n\
72dzE6uKG/vxBbvZ7eW2GLO6ewa9UxlnLVMPfJdpkp/xYXwwcPW2+2YXCge1ujxs\n\
tjrOQJ5HUySh6DkE/kZpx8zwYWm9AaCrsvCIX1thsqgvKy+U5v1FS1L58eGc6s//\n\
9yMgNhU29R0CAwEAAaOByTCBxjAMBgNVHRMEBTADAQH/MB0GA1UdDgQWBBRNVUJN\n\
FzhJPReYT4QSx6dK53CXCTAfBgNVHSMEGDAWgBRNVUJNFzhJPReYT4QSx6dK53CX\n\
CTAPBgNVHQ8BAf8EBQMDB/+AMGUGA1UdJQEB/wRbMFkGCCsGAQUFBwMBBggrBgEF\n\
BQcDAgYIKwYBBQUHAwMGCCsGAQUFBwMEBggrBgEFBQcDCAYIKwYBBQUHAwkGCCsG\n\
AQUFBwMNBggrBgEFBQcDDgYHKwYBBQIDBTANBgkqhkiG9w0BAQsFAAOCAQEAcOLF\n\
ZYdJguj0uxeXB8v3xnUr1AWz9+gwg0URdfNLU2KvF2lsb/uznv6168b3/FcPgezN\n\
Ihl9GqB+RvGwgXS/1UelCGbQiIUdsNxk246P4uOGPIyW32RoJcYPWZcpY+cw11tQ\n\
NOnk994Y5/8ad1DmcxVLLqq5kwpXGWQufV1zOONq8B+mCvcVAmM4vkyF/de56Lwa\n\
sAMpk1p77uhaDnuq2lIR4q3QHX2wGctFid5Q375DRscFQteY01r/dtwBBrMn0wuL\n\
AMNx9ZGD+zAoOUaslpIlEQ+keAxk3jgGMWFMxF81YfhEnXzevSQXWpyek86XUyFL\n\
O9IAQi5pa15gXjSbUg==\n\
-----END CERTIFICATE-----";

char *test_privatekey = "data:,-----BEGIN RSA PRIVATE KEY-----\n\
MIIEowIBAAKCAQEA0Fb60RkFrDsRAIRfLMf2fsiRnw9Agp7yEbUsA4Zg4X+hRhBk\n\
k78fFtagLisWo4LbT4DqKzqQXCvVnOn9a6uT0KsVejk2iaLoBy0fotQznudio3rz\n\
DpTfzP9eRDFVj8RJnl3JJ8P1DULxH7N0TyDBuKZ1ibBfuKjPffqeCEzYKgAc8NG2\n\
FB4m6mXzHhXuKw6W2v0IbHz6KYldsavf04rq7ztw0t67bexSQ4gLSVJEwiohAreg\n\
m0dLvQnap5xd2qn0yETteTL8Y+Ujym1DWAnwDdwA6+0j2a0wkyzOzYQpZMt95NyZ\n\
8kcoqEqO8KAN2HclqfjHlg9iDscbof5x45SQwwIDAQABAoIBAG0dYPeqd0IhHWJ7\n\
8azufbchLMN1pX/D51xG2uptssfnpHuhkkufSZUYi4QipRS2ME6PYhWJ8pmTi6lH\n\
E6cUkbI0KGd/F4U2gPdhNrR9Fxwea5bbifkVF7Gx/ZkRjZJiZ3w9+mCNTQbJDKhh\n\
wITAzzT6WYznhvqbzzBX1fTa6kv0GAQtX7aHKM+XIwkhX2gzU5TU80bvH8aMrT05\n\
tAMGQqkUeRnpo0yucBl4VmTZzd/+X/d2UyXR0my15jE5iH5o+p+E6qTRE9D+MGUd\n\
MQ6Ftj0Untqy1lcog1ZLL6zPlnwcD4jgY5VCYDgvabnrSwymOJapPLsAEdWdq+U5\n\
ec44BMECgYEA/+3qPUrd4XxA517qO3fCGBvf2Gkr7w5ZDeATOTHGuD8QZeK0nxPl\n\
CWhRjdgkqo0fyf1cjczL5XgYayo+YxkO1Z4RUU+8lJAHlVx9izOQo+MTQfkwH4BK\n\
LYlHxMoHJwAOXXoE+dmBaDh5xT0mDUGU750r763L6EFovE4qRBn9hxkCgYEA0GWz\n\
rpOPNxb419WxG9npoQYdCZ5IbmEOGDH3ReggVzWHmW8sqtkqTZm5srcyDpqAc1Gu\n\
paUveMblEBbU+NFJjLWOfwB5PCp8jsrqRgCQSxolShiVkc3Vu3oyzMus9PDge1eo\n\
9mwVGO7ojQKWRu/WVAakENPaAjeyyhv4dqSNnjsCgYEAlwe8yszqoY1k8+U0T0G+\n\
HeIdOCXgkmOiNCj+zyrLvaEhuS6PLq1b5TBVqGJcSPWdQ+MrglbQIKu9pUg5ptt7\n\
wJ5WU+i9PeK9Ruxc/g/BFKYFkFJQjtZzb+nqm3wpul8zGwDN/O/ZiTqCyd3rHbmM\n\
/dZ/viKPCZHIEBAEq0m3LskCgYBndzcAo+5k8ZjWwBfQth5SfhCIp/daJgGzbYtR\n\
P/BenAsY2KOap3tjT8Fsw5usuHSxzIojX6H0Gvu7Qzq11mLn43Q+BeQrRQTWeFRc\n\
MQdy4iZFZXNNEp7dF8yE9VKHwdgSJPGUdxD6chMvf2tRCN6mlS171VLV6wVvZvez\n\
H/vX5QKBgD2Dq/NHpjCpAsECP9awmNF5Akn5WJbRGmegwXIih2mOtgtYYDeuQyxY\n\
ZCrdJFfIUjUVPagshEmUklKhkYMYpzy2PQDVtaVcm6UNFroxT5h+J+KDs1LN1H8G\n\
LsASrzyAg8EpRulwXEfLrWKiu9DKv8bMEgO4Ovgz8zTKJZIFhcac\n\
-----END RSA PRIVATE KEY-----";


DDS_Security_ValidationResult_t
validate_local_identity(
        dds_security_authentication *instance,
        DDS_Security_IdentityHandle *local_identity_handle,
        DDS_Security_GUID_t *adjusted_participant_guid,
        const DDS_Security_DomainId domain_id,
        const DDS_Security_Qos *participant_qos,
        const DDS_Security_GUID_t *candidate_participant_guid,

        DDS_Security_SecurityException *ex);
DDS_Security_boolean
get_identity_token(dds_security_authentication *instance,
                   DDS_Security_IdentityToken *identity_token,
                   const DDS_Security_IdentityHandle handle,
                   DDS_Security_SecurityException *ex);
DDS_Security_boolean
set_permissions_credential_and_token(
        dds_security_authentication *instance,
        const DDS_Security_IdentityHandle handle,
        const DDS_Security_PermissionsCredentialToken *permissions_credential,
        const DDS_Security_PermissionsToken *permissions_token,
        DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
validate_remote_identity(
        dds_security_authentication *instance,
        DDS_Security_IdentityHandle *remote_identity_handle,
        DDS_Security_AuthRequestMessageToken *local_auth_request_token,
        const DDS_Security_AuthRequestMessageToken *remote_auth_request_token,
        const DDS_Security_IdentityHandle local_identity_handle,
        const DDS_Security_IdentityToken *remote_identity_token,
        const DDS_Security_GUID_t *remote_participant_guid,
        DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
begin_handshake_request(
        dds_security_authentication *instance,
        DDS_Security_HandshakeHandle *handshake_handle,
        DDS_Security_HandshakeMessageToken *handshake_message,
        const DDS_Security_IdentityHandle initiator_identity_handle,
        const DDS_Security_IdentityHandle replier_identity_handle,
        const DDS_Security_OctetSeq *serialized_local_participant_data,
        DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
begin_handshake_reply(
        dds_security_authentication *instance,
        DDS_Security_HandshakeHandle *handshake_handle,
        DDS_Security_HandshakeMessageToken *handshake_message_out,
        const DDS_Security_HandshakeMessageToken *handshake_message_in,
        const DDS_Security_IdentityHandle initiator_identity_handle,
        const DDS_Security_IdentityHandle replier_identity_handle,
        const DDS_Security_OctetSeq *serialized_local_participant_data,
        DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
process_handshake(
        dds_security_authentication *instance,
        DDS_Security_HandshakeMessageToken *handshake_message_out,
        const DDS_Security_HandshakeMessageToken *handshake_message_in,
        const DDS_Security_HandshakeHandle handshake_handle,
        DDS_Security_SecurityException *ex);

DDS_Security_SharedSecretHandle get_shared_secret(
        dds_security_authentication *instance,
        const DDS_Security_HandshakeHandle handshake_handle,
        DDS_Security_SecurityException *ex);

DDS_Security_boolean
get_authenticated_peer_credential_token(
        dds_security_authentication *instance,
        DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
        const DDS_Security_HandshakeHandle handshake_handle,
        DDS_Security_SecurityException *ex);


DDS_Security_boolean get_identity_status_token(
        dds_security_authentication *instance,
        DDS_Security_IdentityStatusToken *identity_status_token,
        const DDS_Security_IdentityHandle handle,
        DDS_Security_SecurityException *ex);

DDS_Security_boolean set_listener(dds_security_authentication *instance,
                                  const dds_security_authentication_listener *listener,
                                  DDS_Security_SecurityException *ex);

DDS_Security_boolean return_identity_token(dds_security_authentication *instance,
                                           const DDS_Security_IdentityToken *token,
                                           DDS_Security_SecurityException *ex);

DDS_Security_boolean return_identity_status_token(
        dds_security_authentication *instance,
        const DDS_Security_IdentityStatusToken *token,
        DDS_Security_SecurityException *ex);

DDS_Security_boolean return_authenticated_peer_credential_token(
        dds_security_authentication *instance,
        const DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
        DDS_Security_SecurityException *ex);

DDS_Security_boolean
return_handshake_handle(dds_security_authentication *instance,
                        const DDS_Security_HandshakeHandle handshake_handle,
                        DDS_Security_SecurityException *ex);
DDS_Security_boolean
return_identity_handle(
        dds_security_authentication *instance,
        const DDS_Security_IdentityHandle identity_handle,
        DDS_Security_SecurityException *ex);

DDS_Security_boolean return_sharedsecret_handle(
        dds_security_authentication *instance,
        const DDS_Security_SharedSecretHandle sharedsecret_handle,
        DDS_Security_SecurityException *ex);



#endif /* SECURITY_BUILTIN_PLUGINS_AUTHENTICATION_H_ */
