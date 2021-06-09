Self-signed CA for tests
========================

The keys that live in this directory are all part of a self-signed Certificate Authority (CA) that is used during the tests.

Generating the self-signed CA
-----------------------------

The self-signed CA private key (identity_ca_private_key) and public certificate (identity_ca) can be generated with OpenSSL 1.1.1f by doing the following:

```
$ openssl genrsa -out identity_ca_private_key 2048
$ openssl req -key identity_ca_private_key -new -x509 -days 73000 -sha256 -extensions v3_ca -subj "/C=NL/ST=Some-State/O=Internet Widgits Pty Ltd/CN=CHAM500 root ca" -out identity_ca
```

Generating the valid RSA identity
---------------------------------

The valid RSA identity private key (private_key) and public certificate (identity_certificate) can be generated with OpenSSL 1.1.1f by doing the following:

```
$ openssl genrsa -out private_key 2048
$ openssl req -new -sha256 -key private_key -subj "/C=NL/ST=Some-State/O=Internet Widgits Pty Ltd/CN=CHAM500 cert" -out identity.csr
$ openssl x509 -req -in identity.csr -CA identity_ca -CAkey identity_ca_private_key -CAcreateserial -out identity_certificate -days 73000 -sha256
```

Generating the password-protected identity private key
------------------------------------------------------

The valid RSA identity private key, protected by a password (private_key_w_password) can be generated with OpenSSL 1.1.1f by doing the following:

```
$ openssl rsa -in private_key.pem -out private_key_w_password.pem -aes256 -passout pass:CHAM569
```

Creating a revoked certificate and CRL file
-------------------------------------------

The valid RSA private key (revoked_private_key) and public certificate (revoked_identity_certificate) can be generated with OpenSSL 1.1.1f by doing the following:

```
$ openssl genrsa -out revoked_private_key 2048
$ openssl req -new -sha256 -key revoked_private_key -subj "/C=NL/ST=Some-State/O=Internet Widgits Pty Ltd/CN=CHAM500 cert" -out revoked_identity.csr
$ openssl x509 -req -in revoked_identity.csr -CA identity_ca -CAkey identity_ca_private_key -CAcreateserial -out revoked_identity_certificate -days 73000 -sha256
```

Now the certificate can be revoked with:

```
$ openssl ca -revoke revoked_identity_certificate -keyfile identity_ca_private_key -cert identity_ca -config crl_openssl.conf
$ openssl ca -gencrl -keyfile identity_ca_private_key -cert identity_ca -out crl -config crl_openssl.conf
```
