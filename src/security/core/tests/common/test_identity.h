// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef PLUGIN_SECURITY_CORE_TEST_IDENTITY_H_
#define PLUGIN_SECURITY_CORE_TEST_IDENTITY_H_

#define TEST_IDENTITY_CERTIFICATE_DUMMY "testtext_IdentityCertificate_testtext"
#define TEST_IDENTITY_PRIVATE_KEY_DUMMY "testtext_PrivateKey_testtext"
#define TEST_IDENTITY_CA_CERTIFICATE_DUMMY "testtext_IdentityCA_testtext"

// Identity CAs
#define TEST_IDENTITY_CA1_CERTIFICATE "-----BEGIN CERTIFICATE-----\n\
MIIEYzCCA0ugAwIBAgIUOp5yaGGuh0vaQTZHVPkX5jHoc/4wDQYJKoZIhvcNAQEL\n\
BQAwgcAxCzAJBgNVBAYTAk5MMQswCQYDVQQIDAJPVjEWMBQGA1UEBwwNTG9jYWxp\n\
dHkgTmFtZTETMBEGA1UECwwKRXhhbXBsZSBPVTEjMCEGA1UECgwaRXhhbXBsZSBJ\n\
RCBDQSBPcmdhbml6YXRpb24xFjAUBgNVBAMMDUV4YW1wbGUgSUQgQ0ExOjA4Bgkq\n\
hkiG9w0BCQEWK2F1dGhvcml0eUBjeWNsb25lZGRzc2VjdXJpdHkuYWRsaW5rdGVj\n\
aC5jb20wHhcNMjAwMjI3MTkyMjA1WhcNMzAwMjI0MTkyMjA1WjCBwDELMAkGA1UE\n\
BhMCTkwxCzAJBgNVBAgMAk9WMRYwFAYDVQQHDA1Mb2NhbGl0eSBOYW1lMRMwEQYD\n\
VQQLDApFeGFtcGxlIE9VMSMwIQYDVQQKDBpFeGFtcGxlIElEIENBIE9yZ2FuaXph\n\
dGlvbjEWMBQGA1UEAwwNRXhhbXBsZSBJRCBDQTE6MDgGCSqGSIb3DQEJARYrYXV0\n\
aG9yaXR5QGN5Y2xvbmVkZHNzZWN1cml0eS5hZGxpbmt0ZWNoLmNvbTCCASIwDQYJ\n\
KoZIhvcNAQEBBQADggEPADCCAQoCggEBALKhk7JXUpqJphyOC6oOI00LH49WTtO2\n\
GCgDyJhcRYYAm7APMtmEDH+zptvd34N4eSu03Dc65cB/XN4Lbi2TjolVvKz0hHjz\n\
tzmQT5jTgb1UkJX4NjKGw+RrYe9Ls0kfoAL2kvb12kmd1Oj4TIKMZP9TCrz7Vw8m\n\
cZKQxZ56bLys6cU2XdiTp3v+Ef/vMll4+DINj4ZAMWL3CkT+q1G6ZxHRpFlsIyhc\n\
Q1wX6gxUoY6cQdBA7TehKCCEWz4L1KM1A18ZmCHmjTniU0ssLoiAzsQs4b6Fnw8Z\n\
MLFj8ocwzN5g66gJJWGofakXqX/V24KbGl54WX2X7FYU0tGzR234DXcCAwEAAaNT\n\
MFEwHQYDVR0OBBYEFGeCcK8B74QWCuuCjlSUzOBBUTF5MB8GA1UdIwQYMBaAFGeC\n\
cK8B74QWCuuCjlSUzOBBUTF5MA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL\n\
BQADggEBAJQeMc4XzMFnpQKCb58rzRs3Wt9FmZZS4O596sHxMEewTkEHm5gLYMzF\n\
9JYEdUiLoTurQuIr0KgPi+Q3kliQdLfrVPbdWTmlUDZARR5ir5d1gGHST6qnb3Xi\n\
mG+7nwle9R/hLrtPio+gYRgwJEiS55f6p0/E1wDcc+6numvjCRQ/CGIiJfwD/R+d\n\
pv93YLEfuliZttfBc/apIu6OL4chxF+3QgSw1ltV5nXXqDTGHMRZENkp3Yiolumc\n\
6smL4uA7Q812pVcENi3MLjdJgBS/8DcSBQHspVuXugaKKPDMkJnD0IyLWc8vLXh4\n\
O7JdDrmusJAZA9RsTkinl3DuPfF34Sk=\n\
-----END CERTIFICATE-----"

#define TEST_IDENTITY_CA1_PRIVATE_KEY "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEowIBAAKCAQEAsqGTsldSmommHI4Lqg4jTQsfj1ZO07YYKAPImFxFhgCbsA8y\n\
2YQMf7Om293fg3h5K7TcNzrlwH9c3gtuLZOOiVW8rPSEePO3OZBPmNOBvVSQlfg2\n\
MobD5Gth70uzSR+gAvaS9vXaSZ3U6PhMgoxk/1MKvPtXDyZxkpDFnnpsvKzpxTZd\n\
2JOne/4R/+8yWXj4Mg2PhkAxYvcKRP6rUbpnEdGkWWwjKFxDXBfqDFShjpxB0EDt\n\
N6EoIIRbPgvUozUDXxmYIeaNOeJTSywuiIDOxCzhvoWfDxkwsWPyhzDM3mDrqAkl\n\
Yah9qRepf9XbgpsaXnhZfZfsVhTS0bNHbfgNdwIDAQABAoIBAAylo+9cf1yxojEj\n\
XXAM0DMENpfPZIVYvx0WJ32iCsoSAPPWH6OG1du0vHuUmd6VCP8vLug6I0odulV+\n\
Oa7AY7cVeuZD6Z0mpDJPJVOMpgLhmdsEV9H7+KKTd7uZgHgM5SdQjdcuUOYlZo2Y\n\
BtK3Xe810ezPXrqT3jaiSVuPD2PMO/LH3S+MSynHUZdou+NEr0S5FyX7HT2SUvPg\n\
nEG5KUSE32/1Rnho9BacWKQ/HAoBiS+jMRHOPwu1Q/qS/QLw8HRDEuEoXlVYoNMc\n\
il0r3M25COZVJVJecBqXHAWZqCBsqmNXs2hU1bh8VVKl/CMkG3W+IAR7XzMmw6bi\n\
ZYAvgQECgYEA3pIeB8WptYv7hU0I1FZMfyhwLZDRSTZMlObTIXXsdSVKEllBwKAW\n\
N0G84cyl7q+tOio63tbk1rQRi21O0wTrt16j+0SqDIYy59QhqXhiKAko+nf7cSpy\n\
8h+k+HF5HpxsxcwYPiwO1SywPJ4TuDLIRHXXqschRzNtATrqCBtNeBsCgYEAzXX5\n\
cQfMORsg0Z/K0d/U8JLdK0dnqsjwKt+9L7BAGOSv9Xxf4OT+vK3AlzkTUUOztXy7\n\
3YzpSrHy1Dzu57Dv83BgCFPJxa7jKX4/n+SFYjqxcQVSziQAJjyaa0JsxYkxWC0K\n\
IXg8MXYcgwHL6k/PYblQCJw8Lgtf8J2DtXhZTdUCgYAvt/4uRmfLX7bObqS8+b+u\n\
55mde1YTr0ueBRsxKlpHB3apFm/tf6UjtblsY/cThKDMPq+ehU5M5hB45zemMIDl\n\
MKpRvfgDdWZGpAmPjxrkYIpjoQPM0IASf0xcY9/G+1yqz8ZG1iVb+RfT90RdEq4z\n\
V1yk5cqxvEnboKj6kff7DwKBgQCXnqbcVZ/MyIs4ho4awO4YNpkGNiR3cN9jFEc9\n\
aPh0Jlb/drAee37M6AAG2LS7tJVqqcjNXw5N8/G509mNmxIH+Pa1TnfI7R1v4l27\n\
dd1EtwF44S/RNdnyXaiq3JL+VxbV9i7SsjLhYUL7HplHqWvltuYr5He4luZO3z5x\n\
7YUhnQKBgHmjhmSEPPxZLjPcYJ/z2opXalPJ5OObJ6nM6X4T5LvfhSnANdeeM6yj\n\
gigRY8UlnNYzC5iSt17/VuMeta7I8GaVNUu7WU72Q7sYYlrXevdgl/0OFYayXHlB\n\
sSo6yb9za+C2+5olHEZvs7dIzwDcveoEatds/X4VNrULEwaGbZR0\n\
-----END RSA PRIVATE KEY-----"

#define TEST_IDENTITY_CA2_CERTIFICATE "-----BEGIN CERTIFICATE-----\n\
MIIEbTCCA1WgAwIBAgIUL0mSpPRgzveYTJ8UHSmOIwkIjjYwDQYJKoZIhvcNAQEL\n\
BQAwgcUxCzAJBgNVBAYTAk5MMQswCQYDVQQIDAJPVjEWMBQGA1UEBwwNTG9jYWxp\n\
dHkgTmFtZTETMBEGA1UECwwKRXhhbXBsZSBPVTElMCMGA1UECgwcRXhhbXBsZSBJ\n\
RCBDQSAyIE9yZ2FuaXphdGlvbjEYMBYGA1UEAwwPRXhhbXBsZSBJRCBDQSAyMTsw\n\
OQYJKoZIhvcNAQkBFixhdXRob3JpdHkyQGN5Y2xvbmVkZHNzZWN1cml0eS5hZGxp\n\
bmt0ZWNoLmNvbTAeFw0yMDAyMjcxNjI3MjRaFw0zMDAyMjQxNjI3MjRaMIHFMQsw\n\
CQYDVQQGEwJOTDELMAkGA1UECAwCT1YxFjAUBgNVBAcMDUxvY2FsaXR5IE5hbWUx\n\
EzARBgNVBAsMCkV4YW1wbGUgT1UxJTAjBgNVBAoMHEV4YW1wbGUgSUQgQ0EgMiBP\n\
cmdhbml6YXRpb24xGDAWBgNVBAMMD0V4YW1wbGUgSUQgQ0EgMjE7MDkGCSqGSIb3\n\
DQEJARYsYXV0aG9yaXR5MkBjeWNsb25lZGRzc2VjdXJpdHkuYWRsaW5rdGVjaC5j\n\
b20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDk+ewDf871kPgBqXkm\n\
UEXdf/vqWWoKx3KfJ4N3Gq4vt/cDOMs0xakpqr5uxm787AvbOui4P8QmT8naLhAA\n\
TvHtNGg2LV0ZQtLcVVFsXXsBYDUEbLJYmCBtJU8zSfLLzgtN+z9nVqLthAcVyGhZ\n\
iEkCfXKS4XzwjFUxgrXUM1VSiHHz8DbreQFDTF8mVavZ75HjieuHz1OcSaoIHCIF\n\
mhPDlxRR/qZpc3Y52NZMNRHVPj4Tmc3N4H2eneeoG7nVn0MgNuqbssezeQtUOOoH\n\
DgPGp3xzd8XQxaF5hVIM9E7aL77kw5v4gwccjL5xWC72zzxC3c1ltmbaEcwhHGsu\n\
MR4lAgMBAAGjUzBRMB0GA1UdDgQWBBTTpmGTY5teWrZBA8Sd7kL5Lg/JmjAfBgNV\n\
HSMEGDAWgBTTpmGTY5teWrZBA8Sd7kL5Lg/JmjAPBgNVHRMBAf8EBTADAQH/MA0G\n\
CSqGSIb3DQEBCwUAA4IBAQCbelDJr9sVsYgQSp4yzSOSop5DSOWCweBF56NatcbY\n\
3HUYc4iaH4NcB04WFkUl2XmqVCAM0zbmV0q4HoQikTK5PBHmwxuuD2HhPDWtMeFR\n\
W96BjzGVpV27yaNIPvLwjTVV+A72r4vRvufiFhrMCovRwlWgHY6+gXKfrtyljTZ0\n\
m1mENHOJOQWDXFAXP5yiehSMKy/izKvQ1G1hLErYMMc+sdgF/9X2KaudnTakTW0d\n\
44kXUFKSU7mqV44D12unxCNODclznd31tiJ+70U39AXlR2BzwBzyFzPCh5JYtMog\n\
TwbdLY3LN40gpkDUxIIH115D7ujUKNd8s2gmSHOCm1ar\n\
-----END CERTIFICATE-----"

#define TEST_IDENTITY_CA2_PRIVATE_KEY "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEogIBAAKCAQEA5PnsA3/O9ZD4Aal5JlBF3X/76llqCsdynyeDdxquL7f3AzjL\n\
NMWpKaq+bsZu/OwL2zrouD/EJk/J2i4QAE7x7TRoNi1dGULS3FVRbF17AWA1BGyy\n\
WJggbSVPM0nyy84LTfs/Z1ai7YQHFchoWYhJAn1ykuF88IxVMYK11DNVUohx8/A2\n\
63kBQ0xfJlWr2e+R44nrh89TnEmqCBwiBZoTw5cUUf6maXN2OdjWTDUR1T4+E5nN\n\
zeB9np3nqBu51Z9DIDbqm7LHs3kLVDjqBw4Dxqd8c3fF0MWheYVSDPRO2i++5MOb\n\
+IMHHIy+cVgu9s88Qt3NZbZm2hHMIRxrLjEeJQIDAQABAoIBABBUirKNMPNujWGA\n\
9rT20KTFde/2xItUQiZ7qPKbooSguCswp71xw2jHVqGL4WqEYywVfXd2hMS+uASp\n\
eFatSq/CJxSGE7ezflpcc1wpJpaoh99y6R1MbDOcj5N22KwUW9YJ7zGtih0qZ170\n\
VgzcnWhiDgPPtRtqxsCrM9CYgKNMGU6M9CFPX3PKDudKVU5fmi9bhtCqQaLhImbs\n\
aQO3y4yI0af4KSQSur+eqeB/z7V39BEo1LfqaVQd1e9ItEYnTg8TaSaCshS4H8UG\n\
Yx/pGhnxgn8+5LFL2K635Mb99OLb0hUwIOAbuoAuTlKijit0uGEJe/+DjbkcgZ5d\n\
VB9I8UECgYEA/56Em8M6N6mTUN3WN+U8NSLltjSIL8kemK30dJ56aUsTiy4h0jNa\n\
Jda7BeRPQcf9pnQpgFkV7XoKIbfTIqOhqD8XAvJL4//+VMmH2q/R2Xf6e0/CIEUe\n\
3K74QyRVazx+tt+NOafCwjU9bA7ebjwQVsb+dPAS6kOWxTCZFzCgFk0CgYEA5VE+\n\
U/C8D9zmJjL1uc4XkBNAg/dQNybQ9DX2Ku5dKME6yg4u7Lxnl3X9asvtQAsUeOPa\n\
dKGkQ8NZfnSvXYd04n/FTRohFCaYz3RWIbo/q4KCsnJk6uAUM9YFeQGqZdEjO3Mu\n\
Yk1uhHFl+C4Q/InzYEs+QwtMOS7XVMa5vm6OQzkCgYAR+xKU6ly0AaetLo2dDPD5\n\
Q+UotfVGdz1BvCrP8T3nHjLXvXz/jkEvHDW3qmGw3OKIzO8Gaj3SoJ0J1iZx71S1\n\
wwpZWLXh6eX4DN0Tkv6N75SdC/U50+Lh3yTzhCDGFFFNh9glUBmxE5Gogjs/QdZc\n\
ZE8N5r1N4Uc/w7VhHjiEmQKBgHMA2pQ4P+horRdtKSTEwbZkoU9NYXI3SkWfJlSD\n\
dD7zISuiD1B0cDNaXfwIR3R92geCpdUmF35QYvpzRFtQioLo9ybiusIjVTF9M5D4\n\
mePGsQsTKZ9NP3R7mgUEm9MyHkw7SIDOOmW7hRsA503vVRnuwkvXR6PJ5P3EJ/Tj\n\
9v6pAoGAfVTCJkf0aZ7KjV5GQ33l8uH9dEBzRJ4xOjGpeVBTm+L5N4ejcrQ4p9t0\n\
JQr8hQHEp7tnHZ9H8DuIIQVDUJdrRa1qO+TNQiOdPLRNofXHuFDNIoj4+bP5ISnL\n\
dImrEylWjCLbiiOpIbBmAQLb55xYsFzkdRCW3AlmldSVUW96yfU=\n\
-----END RSA PRIVATE KEY-----"

// Permissions CAs
#define TEST_PERMISSIONS_CA_CERTIFICATE "-----BEGIN CERTIFICATE-----\n\
MIIEbzCCA1egAwIBAgIUfoby6818hlJQ+41KUHiM6BZll/0wDQYJKoZIhvcNAQEL\n\
BQAwgcYxCzAJBgNVBAYTAk5MMQswCQYDVQQIDAJPVjEWMBQGA1UEBwwNTG9jYWxp\n\
dHkgTmFtZTETMBEGA1UECwwKRXhhbXBsZSBPVTEgMB4GA1UECgwXRXhhbXBsZSBD\n\
QSBPcmdhbml6YXRpb24xHzAdBgNVBAMMFkV4YW1wbGUgUGVybWlzc2lvbnMgQ0Ex\n\
OjA4BgkqhkiG9w0BCQEWK2F1dGhvcml0eUBjeWNsb25lZGRzc2VjdXJpdHkuYWRs\n\
aW5rdGVjaC5jb20wHhcNMjAwMjI3MTM0ODA5WhcNMzAwMjI0MTM0ODA5WjCBxjEL\n\
MAkGA1UEBhMCTkwxCzAJBgNVBAgMAk9WMRYwFAYDVQQHDA1Mb2NhbGl0eSBOYW1l\n\
MRMwEQYDVQQLDApFeGFtcGxlIE9VMSAwHgYDVQQKDBdFeGFtcGxlIENBIE9yZ2Fu\n\
aXphdGlvbjEfMB0GA1UEAwwWRXhhbXBsZSBQZXJtaXNzaW9ucyBDQTE6MDgGCSqG\n\
SIb3DQEJARYrYXV0aG9yaXR5QGN5Y2xvbmVkZHNzZWN1cml0eS5hZGxpbmt0ZWNo\n\
LmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANNWwyrW3J+TCyaZ\n\
H77q+29GGqFsYP5rv9cpcL/TMDNccsPYY+1RA1K+zMRYo1LG8VdJNtJlhxE+tmEb\n\
KxsVUTtoj8zbLVU4P4g0gIh6U7LMv5lUEZ3XYKWvYrbZTFMof2rXQYGXPO7pFnvb\n\
NAbnMiLmagRKxKJ91kq4utuMG3U6rkCA7i2S8cEISNO3gIpFa0IZJ8yS8wDlKa/L\n\
GxL90BYasLsSA6tw/69OIiUUYqpMRD+xxyyTkMO37VjmdiFLHa/dxO8HH0t3Q0U0\n\
AgZP9uwYTgZpN+2UEFnjv3BDIydc3Wa0UaSdxLtHXMPvg3sRuH9CTqr4Le7/3uTY\n\
ehYKgd0CAwEAAaNTMFEwHQYDVR0OBBYEFFi4pK986ZSB0BLiMm8ivu6AUxYPMB8G\n\
A1UdIwQYMBaAFFi4pK986ZSB0BLiMm8ivu6AUxYPMA8GA1UdEwEB/wQFMAMBAf8w\n\
DQYJKoZIhvcNAQELBQADggEBAHYLaJVWrLHg+62jC8yIz9dbECIroX9Gb7Ll937H\n\
Mum6Hj4wlImrifMVV3iORWBrBLvtTtn0Zno3mwfjLRQtkjOih71eJT+6//B7CT7n\n\
oULJYVq8IRGErbKtmXULnxTajFApzO0v4hSu7rWj/Jfhil0TX7QgKNpgKzjYodWz\n\
3oGGtchxvw3+v9wdIWD5Cj0bk/VMCQCaBV0anvyga7d4k8/zPF7nW2Z9jNfKsVD1\n\
piFa+Yd4zN6XOPPKiFXfLD7ht9i2gG25iS+d95tKg1DfjnRD7u0BJSOAPerxGtN/\n\
wf43qY1XzUoE2FBJ9QJGOA/02ffaUMOwSzICF/ShctH+Knk=\n\
-----END CERTIFICATE-----"

#define TEST_PERMISSIONS_CA_PRIVATE_KEY "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpAIBAAKCAQEA01bDKtbcn5MLJpkfvur7b0YaoWxg/mu/1ylwv9MwM1xyw9hj\n\
7VEDUr7MxFijUsbxV0k20mWHET62YRsrGxVRO2iPzNstVTg/iDSAiHpTssy/mVQR\n\
nddgpa9ittlMUyh/atdBgZc87ukWe9s0BucyIuZqBErEon3WSri624wbdTquQIDu\n\
LZLxwQhI07eAikVrQhknzJLzAOUpr8sbEv3QFhqwuxIDq3D/r04iJRRiqkxEP7HH\n\
LJOQw7ftWOZ2IUsdr93E7wcfS3dDRTQCBk/27BhOBmk37ZQQWeO/cEMjJ1zdZrRR\n\
pJ3Eu0dcw++DexG4f0JOqvgt7v/e5Nh6FgqB3QIDAQABAoIBABFHMKGZ+2OYc/rt\n\
3eiP8YqBYr/7ylpCmOaQXsVwEKrCTiew00qdqvXi337V+FRWK3kFZVQCNO61/9ck\n\
j3uhXIjM3aTT7nrfJGKQWEnQJnOhxbBVbTNIXoBtPFbSoSjTUMd9Xb+oi7TEna/2\n\
leRSloi/6b78FeNrAlANlklIxR3qTjRSxjGYVfukCWsKq3uFfWM4Wp9N1B1gsyRo\n\
/SH2jOu0XTLNdajggtBKcFoqxVIiaetERKVRRid7pW0zwuYS5Zwv5Wtl3XMbUuAn\n\
VGesMeCKAGpwkLjmvXKBE5setnd7cWBKdVKddYDkzbDvU7X6QEHFnac6m6OQ2P62\n\
QfkO94ECgYEA70tV55AreDnPQEpf698ZjA8pYvF90GfGx/Y4oYWU/s0IlD6Pfbsr\n\
qkRu+1I+SUNZWARhirXmJzuOmJYUQRteCEq+6RPJzn5Jl9MtipOBAjI0h589dbAB\n\
8m/BRk+bEZKCXLgVa0TyZ/gd/wDBxB+qd+bPep8nAl4krMWK9W1+DLECgYEA4hfP\n\
EwUPMwHrGlq0oRUA08ssQ8XxVCKWeS3cLAJjO6EdJyIUm/8S/UZPBPeSkGyZeld+\n\
fY7z9ZV0HA338p5BYYDCqgJC6b5Ud5UV0yLkq01v6b0H3nSjTPcbA61l9laN0vhm\n\
QJ/xTiAHgsGBbOx2VtwDoE8T1AbAaamcapqNYu0CgYAXCiPdRc5JpxdDU2Xk6fgl\n\
uhf8BNBeTn+fJR/SvW/ZEJiw3U0nh+vuWuRsokCJAUkK5nEVz+m3AU77dgfBNQda\n\
uQeknVki3pnrWlPaMdWMBpV0MWrTd/zYANaVFHkTug1/K+I0D9FfHU6WDNabMYlS\n\
PhDf947j9XiGggadFsu6IQKBgQC6dgpIVFbZqU5cuMvZQToicaA68Kd7zN6uZ7z5\n\
6qouRkyFtpyqnq3pha+rmAYe6AGXnUrrgBcAxdYxQO/o/s1K/WcN0LmgjmCZErIi\n\
I9fU0xNmAIjZ1PXMhsqXuMyrYWyrvkKOL5pR5SZsluwHieh68A5pim3+4eaT/dbL\n\
MFVEbQKBgQDfkeApSfKATXJpYIV/tiGjmRkkYoZ6NVar92npjlb72jaA4V0gHmuD\n\
9ttypXOJPB/5zMa5uL6drp3CLv/GcWekUiUUXuyKQpcxZWqxf/leh9gTgGDAH/k4\n\
4+zX4HLEzTmoOc0cqzi4w6pTIj29BOV5QpnnyUGyvj8NGNSdFvZFSQ==\n\
-----END RSA PRIVATE KEY-----"


// Identities

// created with TEST_IDENTITY_CA1_CERTIFICATE
#define TEST_IDENTITY1_CERTIFICATE "-----BEGIN CERTIFICATE-----\n\
MIIEDTCCAvUCFHZ4yXyk/9yeMxgHs6Ib0bLKhXYuMA0GCSqGSIb3DQEBCwUAMIHA\n\
MQswCQYDVQQGEwJOTDELMAkGA1UECAwCT1YxFjAUBgNVBAcMDUxvY2FsaXR5IE5h\n\
bWUxEzARBgNVBAsMCkV4YW1wbGUgT1UxIzAhBgNVBAoMGkV4YW1wbGUgSUQgQ0Eg\n\
T3JnYW5pemF0aW9uMRYwFAYDVQQDDA1FeGFtcGxlIElEIENBMTowOAYJKoZIhvcN\n\
AQkBFithdXRob3JpdHlAY3ljbG9uZWRkc3NlY3VyaXR5LmFkbGlua3RlY2guY29t\n\
MB4XDTIwMDIyNzE5MjQwMVoXDTMwMDIyNDE5MjQwMVowgcQxCzAJBgNVBAYTAk5M\n\
MQswCQYDVQQIDAJPVjEWMBQGA1UEBwwNTG9jYWxpdHkgTmFtZTEhMB8GA1UECwwY\n\
T3JnYW5pemF0aW9uYWwgVW5pdCBOYW1lMR0wGwYDVQQKDBRFeGFtcGxlIE9yZ2Fu\n\
aXphdGlvbjEWMBQGA1UEAwwNQWxpY2UgRXhhbXBsZTE2MDQGCSqGSIb3DQEJARYn\n\
YWxpY2VAY3ljbG9uZWRkc3NlY3VyaXR5LmFkbGlua3RlY2guY29tMIIBIjANBgkq\n\
hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA5mEhLZIP2ko1bRJyJCwbnvhIpXFv6GOh\n\
nvuS5v8tsTju40O62NNQmKT/my1QVKiUu7OoWZtLNBebgxgJ851eQ4TBRXy/f2jG\n\
kLPYM22dohLTblVCpGutn+Itw3QRM3nkne7Sk8O6FP6NH6Y+7gkjxy5kI3GvhuIC\n\
uBIzAV4dHK+hPlCn/Z+W33W71/ZAmnmI+2GaWiu5tjAQyFcmqWbi0BD7TWqBqidZ\n\
2n7LTImUtp8NrYLfhzvgNLr9BZe7uf+T3mgdwcHtfi98GA94Lo6lqGeygiwig746\n\
Y5uW4c6whsbd6riJ8FG1l8O86Ump4bSKChxjeoTLj4M4KX615kYa4QIDAQABMA0G\n\
CSqGSIb3DQEBCwUAA4IBAQAM2g7v3FaA+d1zDkvDF5emCRL+R9H8pgimEOENrZTV\n\
iK/kl8Hm7xkO7/LZ3y/kXpQpth8FtFS6LsZBAXPYabfADeDFVImnOD6UbWewwHQR\n\
01gxkmYL/1nco/g3AsX/Ledh2ihwClGp+d6vNm5xF+Gw8Ux0YvH/aHy4RKg7mE/S\n\
nonfHWRlT2tw1OtohTVhmBn00Jvj0IzSAiNvpmZHVRLYL9JRb5awYSX5XGetpoFM\n\
VwzWIaZ06idvCtPKTfP71jJypV3+I2g5PNqranbuMv5nNAKZq1QlSB07f2Z1VIu6\n\
6jeSZSADfm73qnE2Kj1PiZkPn0Wu+K24GXCvdILATcUS\n\
-----END CERTIFICATE-----"

#define TEST_IDENTITY1_PRIVATE_KEY "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpQIBAAKCAQEA5mEhLZIP2ko1bRJyJCwbnvhIpXFv6GOhnvuS5v8tsTju40O6\n\
2NNQmKT/my1QVKiUu7OoWZtLNBebgxgJ851eQ4TBRXy/f2jGkLPYM22dohLTblVC\n\
pGutn+Itw3QRM3nkne7Sk8O6FP6NH6Y+7gkjxy5kI3GvhuICuBIzAV4dHK+hPlCn\n\
/Z+W33W71/ZAmnmI+2GaWiu5tjAQyFcmqWbi0BD7TWqBqidZ2n7LTImUtp8NrYLf\n\
hzvgNLr9BZe7uf+T3mgdwcHtfi98GA94Lo6lqGeygiwig746Y5uW4c6whsbd6riJ\n\
8FG1l8O86Ump4bSKChxjeoTLj4M4KX615kYa4QIDAQABAoIBAAtNMoJ4ytxLjaln\n\
IUBTBZvb1DyBfxroYFJbRw6b8BLklxuBBBaE70w9s+hZ5bnxdzJqEtUqgBrzGYbp\n\
0/smeixXw99zyjUm367Tk8SaGQSNZd/gwN8uBRt1zgbrl7htv2BcCeqDzIohHq0x\n\
y56DxkSMKw9uEU1NoxKCmgv0IPt6LlvjCwFhDv8iLu4lvu61F+ovVYIM6UXJJH0G\n\
bHcJ1XnFBj5jCJFAWZRq7KxBgc4K3DlG+J7JcGEz89ZnZfGwcIiLqJ4rbU7E0ZE8\n\
LslIHOwodtMDReIRWl6wEYmvd3mQizTXj2EWlRywQ/P3yFlxuHsGxPtRxdWoyXDc\n\
Ii7GZK0CgYEA9KA+uEAMA5jZK0h1EMFoTiOIRe0x8CjlrHg4l0zU0ElcMeUXwoci\n\
XqM0sjARiNgqkcMaONCb5bKgyxncWyWcamUxgp+bi2FUQIlBKHb56TCioPP0zzc6\n\
yCiQ2cA8QW9PjL0WScJz3bCzeXrQceGZenDpPyphYE7SIUaRAOlMTMMCgYEA8RdP\n\
QfYbOrcwgZB8ZycFE7lpZibe7Wh4UI1b/ipNZKcncr2pOZR+gVNv6eDQbV4z9xZY\n\
5K6oU3rUcFHf0ZAi9xIpNzcq9q4+qOGO2OCEZX5tewXjKw9rwyDPUbv3yToFyZ9w\n\
YwEKLfUgnYzpd5qn2NXa/pAZIoTh5ILF+EezD4sCgYEAr2lg0BoNA19NCn5wg01M\n\
kAtmok3Nq1qIJr4mRkfvqlOQaq7N9M2V1arOFJ/nUus+yzrNyMO9pl4Kctjea/Vy\n\
TdC2SeZNUQq/sW86a9u0pIQdebC1cQk3e2OrSplQG9PHhTHpk4Z+Mw+MAqYQZjjR\n\
Jz1j48lt/fNHNlk1jSO9dKUCgYEAuYkJuqZuMBJ4Zs1Nn4ic1KAUp8N0Pdnu9XbD\n\
++aMJtCogBnLWH+Zl2chsigL3o7niNiO0nZDHfNh94pap4i4D9HPHCn9i1du60Ki\n\
Tu8BlKXmFQ3j0+iLMuBWC/2O5DId8BseP2K2dcW2MukVZrEDSNDTNqKoZTNEMDof\n\
pkFvYJ8CgYEA6du9DFFZzIp5tuSIfVqCq5oxiNrRGJ/EpJEneBexrX56cC90sqLM\n\
ecxDkgEVC592b004K1mjak3w6O0jYozQM1uvve38nZyMgtbMo3ORCtAO6Xszj8Oj\n\
yNw1+km1Zy6EWdFEMciEFlbRwWVmDfE/um9LZsSWbmuWAOTww9GBDhc=\n\
-----END RSA PRIVATE KEY-----"


// created with TEST_IDENTITY_CA2_CERTIFICATE
#define TEST_IDENTITY2_CERTIFICATE "-----BEGIN CERTIFICATE-----\n\
MIIEDjCCAvYCFDEZQzcfGKK8IKNyH+AdNSjdyVgnMA0GCSqGSIb3DQEBCwUAMIHF\n\
MQswCQYDVQQGEwJOTDELMAkGA1UECAwCT1YxFjAUBgNVBAcMDUxvY2FsaXR5IE5h\n\
bWUxEzARBgNVBAsMCkV4YW1wbGUgT1UxJTAjBgNVBAoMHEV4YW1wbGUgSUQgQ0Eg\n\
MiBPcmdhbml6YXRpb24xGDAWBgNVBAMMD0V4YW1wbGUgSUQgQ0EgMjE7MDkGCSqG\n\
SIb3DQEJARYsYXV0aG9yaXR5MkBjeWNsb25lZGRzc2VjdXJpdHkuYWRsaW5rdGVj\n\
aC5jb20wHhcNMjAwMjI3MTkyNjIwWhcNMzAwMjI0MTkyNjIwWjCBwDELMAkGA1UE\n\
BhMCTkwxCzAJBgNVBAgMAk9WMRYwFAYDVQQHDA1Mb2NhbGl0eSBOYW1lMSEwHwYD\n\
VQQLDBhPcmdhbml6YXRpb25hbCBVbml0IE5hbWUxHTAbBgNVBAoMFEV4YW1wbGUg\n\
T3JnYW5pemF0aW9uMRQwEgYDVQQDDAtCb2IgRXhhbXBsZTE0MDIGCSqGSIb3DQEJ\n\
ARYlYm9iQGN5Y2xvbmVkZHNzZWN1cml0eS5hZGxpbmt0ZWNoLmNvbTCCASIwDQYJ\n\
KoZIhvcNAQEBBQADggEPADCCAQoCggEBAMAQM9eN0zjTTdZALCTijog0oqx/kqnW\n\
VtVWjV/c34OyPvUuH/DNRH6Cr0fI76UiooLD9nfvHe52X8oZH8WqNW7m7g7dMliu\n\
DJD3yVpdLRmTTgl40ES8MTqmdb2y8ut70MJf5nUz0EQs9lXvnT0ru0B2CfyubiPt\n\
aLSfyDoVBkRLbfzeqaNEQe7Ta6mQKZOckb6BHcaInb9GYEsU+OyOHuf2tCVNnRIH\n\
ALiTPbA7rRS/J7ICS904/qz7w6km9Ta/oYQI5n0np64L+HqgtYZgIlVURW9grg2p\n\
BuaX+xnJdRZbLQ0YYs+Gpmc1Vnykd+c2b0KP7zyHf8WFk9vV5W1ah2sCAwEAATAN\n\
BgkqhkiG9w0BAQsFAAOCAQEA1RHDhLZf/UOR+azuH2lvD7ioSGRtdaOdJchSPdMk\n\
v1q74PsHgm4/QAadVPdzvaIVV9kDfo6kGMzh+dCtq69EqVOavw1WUDo/NfuVSbgA\n\
W7yeA39o3ROMO9UgbE5T3BPLq/XSXdIisP9OA4uXCnt22JELJaSv4m59FHg5gnQ7\n\
2qOWRM7hi/+cQwescE+lDRw7PUzW8SS1HkQA1DmdtIIpWVuvYj7EPUNQX3jIetn8\n\
AuPUgPJObxJhxJSC4p5qy37pYZHiNH1wG/+BDHgZo3wNwFsWqxabKziGB8XU3INc\n\
USI3GDWM2jjElMSnDCj4ChM5wFbwWrqwdOEzeGWBWbo3hQ==\n\
-----END CERTIFICATE-----"

#define TEST_IDENTITY2_PRIVATE_KEY "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpQIBAAKCAQEAwBAz143TONNN1kAsJOKOiDSirH+SqdZW1VaNX9zfg7I+9S4f\n\
8M1EfoKvR8jvpSKigsP2d+8d7nZfyhkfxao1bubuDt0yWK4MkPfJWl0tGZNOCXjQ\n\
RLwxOqZ1vbLy63vQwl/mdTPQRCz2Ve+dPSu7QHYJ/K5uI+1otJ/IOhUGREtt/N6p\n\
o0RB7tNrqZApk5yRvoEdxoidv0ZgSxT47I4e5/a0JU2dEgcAuJM9sDutFL8nsgJL\n\
3Tj+rPvDqSb1Nr+hhAjmfSenrgv4eqC1hmAiVVRFb2CuDakG5pf7Gcl1FlstDRhi\n\
z4amZzVWfKR35zZvQo/vPId/xYWT29XlbVqHawIDAQABAoIBAFNm9cw15zI2+AcA\n\
yOqfgzt8d+OmZl7gF8b+lde6B0meHp7Dj9U2nfa98zWd+QrhtmZIiH/eU0YZG1Gc\n\
hWKFnjxxhZDo1xMRSZ2uLD7UVWBUyj9suiwO+OW6IUjmK3y8wJOXp3DftiHU0IfS\n\
zJoiombEm2Ohr2xkjOJavE0UkisXQauc3K5AKv9coW9W6hzZf330Sm4sokmC5D3B\n\
GcO/Keof2k2sFuv56wXPi9eGuXCEB2trhHhrxqncvb/fbRwpG1ELQsvZBnyuNNnY\n\
FQcLYl52gkttP6EGvRPw1DFbQwsAJKnXBC7ddJaAl+JoKYAcGTt0+mRm0Z8ltzWl\n\
c6uZQsECgYEA4NGiUMNq9kSn/6tQyPcsrphJ5uobu/svLaBipZ0vv2yQP3wQ5NPA\n\
06KjwSm8gg8BLi0LCKplSxY09TwgUsc9unRkDTQ/eflgjdGA76lFKOjvLQdutxn7\n\
eYNbx81WMY6E6n4y6K+2szpqW+Ds1At4ORRvweJWdFyc01bTqWNeuYsCgYEA2rOO\n\
Ye6H2VixUfXzXwevBd4QyFJITW46WqnbYDFcUzf9pYBZfZoHU0YJqolDIhHhtHnG\n\
soRi0Uk5P9D7Lvu+ZHAGQJrdmNELOMoqMNOqXcAdvK44qLLMwaLC8PS2zDIATrhZ\n\
nc0TbeZJC8MynfIpxDsBVVMOa8u4eHRFdpk8ZaECgYEAlzuuCtJKQ7vPn2dpAqdz\n\
gUekfxeA7KV+CR1Y/ruMgSLQrkQRQT1I+5Tuv2QKERty2dMnFv85AJfBrC50N/sb\n\
hTAClfdNtAmTcBM8vvuJMInxSsMzMSzjQ8yfkvqIPvH2a5/VMz3wkwR6w6+84K+O\n\
gidDPpO5QLGENY6097+G2x0CgYEAk7cdX0YGGaZPNiWiOLhu3c6slTEGRs5BucTq\n\
OGF+k3LI7kTvrOchNXyjwLyvTE65nPV3YFIMkIEdmt3jGkvMv/fuMSqoq7PeGYBq\n\
2MnOUz4Ul8Ew4bjKlasCck9HPEo1bPYVCYFfMyaMhdZU1NugnDqiXugXYHWb5jfa\n\
Rw2e/qECgYEA3PvLLHklsRts6P37iSwUmDnkiopSSPfVdGXpDDGD/RbLpG6cSLRm\n\
uh5n9+nxa2YXi0+LMLQvGpCSWk2k2qaRPpe2mahy9yAYrLhfDDcuGpSvw5BBJ3qw\n\
mi1HgIUzXZTRBNamYCltJWYnN0hOlSL6vcHgeJ9y1gSDh0QqB2BG8HY=\n\
-----END RSA PRIVATE KEY-----"


// created with TEST_IDENTITY_CA1_CERTIFICATE
#define TEST_IDENTITY3_CERTIFICATE "-----BEGIN CERTIFICATE-----\n\
MIIEDTCCAvUCFHZ4yXyk/9yeMxgHs6Ib0bLKhXYvMA0GCSqGSIb3DQEBCwUAMIHA\n\
MQswCQYDVQQGEwJOTDELMAkGA1UECAwCT1YxFjAUBgNVBAcMDUxvY2FsaXR5IE5h\n\
bWUxEzARBgNVBAsMCkV4YW1wbGUgT1UxIzAhBgNVBAoMGkV4YW1wbGUgSUQgQ0Eg\n\
T3JnYW5pemF0aW9uMRYwFAYDVQQDDA1FeGFtcGxlIElEIENBMTowOAYJKoZIhvcN\n\
AQkBFithdXRob3JpdHlAY3ljbG9uZWRkc3NlY3VyaXR5LmFkbGlua3RlY2guY29t\n\
MB4XDTIwMDMwNTEzMTczN1oXDTMwMDMwMzEzMTczN1owgcQxCzAJBgNVBAYTAk5M\n\
MQswCQYDVQQIDAJPVjEWMBQGA1UEBwwNTG9jYWxpdHkgTmFtZTEhMB8GA1UECwwY\n\
T3JnYW5pemF0aW9uYWwgVW5pdCBOYW1lMR0wGwYDVQQKDBRFeGFtcGxlIE9yZ2Fu\n\
aXphdGlvbjEWMBQGA1UEAwwNQ2Fyb2wgRXhhbXBsZTE2MDQGCSqGSIb3DQEJARYn\n\
Y2Fyb2xAY3ljbG9uZWRkc3NlY3VyaXR5LmFkbGlua3RlY2guY29tMIIBIjANBgkq\n\
hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0QduVwXptfjiwkvbn5aXuIpwZ9aWOqmj\n\
e36qNknnS0mng1zPhzi4RLAl6CUxa6E5bkfjGZxFfYefDNk3ynzerEotFa1f5++b\n\
aY73hs2ecfz+9ofjqR2fsroxOFwFF9JLbeWTDPS2mf5yE0Ci2+ctq6Ep4jDeHNui\n\
WpSOY8OoIEWq4PD/R/VGJSiHSG+OjOUN7gwuxta0yglFeyHBdzr8mDZiejj1KYBD\n\
AzuQrtaibHNtGBo3VGFvPKs85mK/Swv1GoXxcy1uBU1Yup9JLq3Ds8R5YYecSlXk\n\
77EmZl4dgoScbt4NKTPuo8t803Ph3PYQCggILhlaEwjpfd1YTFLxOQIDAQABMA0G\n\
CSqGSIb3DQEBCwUAA4IBAQAtV57Zc5dV9+z51zTOtghNZrFGJ48xJhnXddMVJ1Yh\n\
08uoODRSRJHXNxMrlSRMeZ+CNkvd/QmqzOYvok3EqusOXFNU9qfmc3DToU/DDqkf\n\
PMEpc9lPLTjmm6MfQgjyT5pDDNPUV9Io1s2o492ozr87ULyVf6I2bNu2NnVv2IzE\n\
j9Mz/L7TkcgJgbdDl+21CR3NRA1PpxFB/PcM+zy4C2XoFv/qcF5pkcBpkyNjva9m\n\
xmjSJWMoIVzXk6apPsRGCJLFJ3uuj+9K6POo/xgAkrbgvZF0i0yAJSTvSQmg6x2S\n\
FMxE89kC7Npg+fQF15aaNEn4tuQiz0WW9pq1wSTXjoqj\n\
-----END CERTIFICATE-----"

#define TEST_IDENTITY3_PRIVATE_KEY "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpAIBAAKCAQEA0QduVwXptfjiwkvbn5aXuIpwZ9aWOqmje36qNknnS0mng1zP\n\
hzi4RLAl6CUxa6E5bkfjGZxFfYefDNk3ynzerEotFa1f5++baY73hs2ecfz+9ofj\n\
qR2fsroxOFwFF9JLbeWTDPS2mf5yE0Ci2+ctq6Ep4jDeHNuiWpSOY8OoIEWq4PD/\n\
R/VGJSiHSG+OjOUN7gwuxta0yglFeyHBdzr8mDZiejj1KYBDAzuQrtaibHNtGBo3\n\
VGFvPKs85mK/Swv1GoXxcy1uBU1Yup9JLq3Ds8R5YYecSlXk77EmZl4dgoScbt4N\n\
KTPuo8t803Ph3PYQCggILhlaEwjpfd1YTFLxOQIDAQABAoIBAGSpyHh+L3vj/QgG\n\
0iB7vFsxyEopbDWaBlHtwPjeBFYchWBcrNB4/zyM++RdLPyhKvAyDGsD9+8pBe6B\n\
GT4Zfn7IRgf/c4VVvalLIWc41IoehYaiEIAb9RF0W0nB/u3m505oVbXSj7F/eN5O\n\
rV9raHIT7gCw+fY5y2kFy8C9s9S9+VzzYOzIZPWSh6Plc/eI2niSVt+MDufDeVOR\n\
Ug6Z54lpXkwqv0Pz8F7ELyRGBvUW5UAvgyprvXgSYz1VNeHr3fmLX/O4rGktk02x\n\
bAFxvaNV/JEK1fLFWLZ8TJVGsni+uYu/zkvXdxw7gphdoM77UKeZ00udg7orYUhW\n\
6MwsuRUCgYEA7KzanZVPfbL4TMCpS6EJUO8H9lebH0srzqKNJD3/NL2JPebRNjZA\n\
niH6vXD773/8IYlFTAXe0npVApZETKVzP4LNvsSVtjNvqXKdRM0cT47Dc4Y16kn/\n\
X9Pd/ff+HH93T8Pcpguovw8QU7nxlf5dCvlnWrCj84Dw1ZToS2NLWAcCgYEA4hiy\n\
nz6xvbkkUc5oHWxJIHxSLrOvMsyLMNp9UlUxgrxGqth0yQsFZwinPzs/y8aUVKyi\n\
bHJlL35fpHeuna0V054E5vyeOOM7eLLFToDITS1m91hsl6amMW8iup/HTZhSemt7\n\
tEn4mWlINXyP47MWfOr4oQ1KDzDCA3JFfzjInL8CgYBRulL3zcKYbn/tyS3s7twP\n\
taszNwdbJBMplNpWZI5HQRguZxFhvhRMRwGV/3kQOErxrbxfRzutxQ6sCQXmzc9h\n\
ZCL2OF5Wf6aUhf6m7olTM8JslzDxCcKE7d2fwM5gOugRhFoigK4x49rIftJc8Gxi\n\
yMMW/x5ujN0ddAFPXyd6awKBgQCypX8lsnzwgsR+2w+LCA+z2md5PULWaaYlgM36\n\
6xPG0AsqXQPSAqJPKhg0LxWWZp63VPy1oaHv5/OcWXCgZ63SWo5XEQ3Xtzw7f03F\n\
XJ5n1NMB511Oaj/w2XZgbXUmC5BH6HuDFduXJAgJMxXifZPsOiEf6Ac3f3gdDwJ4\n\
pp5kswKBgQDNUI3uzqw8J3e81tTAn2U8eyHuQxi8swv6K4Dx+sqCKpxkFcYvDLQl\n\
qI+v234hvmZN3CmGPCY01aZl3NUUFKx9fvwweYG/vicCsA2XKwnmaSWTTrT62vlY\n\
S1cWlJlUjw59ZhAqgD1pe4r0suRQ6e1OT/pByTKz1BxE/lwZftpauA==\n\
-----END RSA PRIVATE KEY-----"

// revoked identity from CA1
#define TEST_REVOKED_IDENTITY_CERTIFICATE "-----BEGIN CERTIFICATE-----\n\
MIID1zCCAr8CFDbu7X0vTo6MnlN4zIk4ZjM3V0PGMA0GCSqGSIb3DQEBCwUAMIHA\n\
MQswCQYDVQQGEwJOTDELMAkGA1UECAwCT1YxFjAUBgNVBAcMDUxvY2FsaXR5IE5h\n\
bWUxEzARBgNVBAsMCkV4YW1wbGUgT1UxIzAhBgNVBAoMGkV4YW1wbGUgSUQgQ0Eg\n\
T3JnYW5pemF0aW9uMRYwFAYDVQQDDA1FeGFtcGxlIElEIENBMTowOAYJKoZIhvcN\n\
AQkBFithdXRob3JpdHlAY3ljbG9uZWRkc3NlY3VyaXR5LmFkbGlua3RlY2guY29t\n\
MCAXDTIxMDYxNDE4NTAzNFoYDzIyMjEwNDI3MTg1MDM0WjCBjDELMAkGA1UEBhMC\n\
TkwxCzAJBgNVBAgMAk9WMRYwFAYDVQQHDA1Mb2NhbGl0eSBOYW1lMSEwHwYDVQQL\n\
DBhPcmdhbml6YXRpb25hbCBVbml0IE5hbWUxHTAbBgNVBAoMFEV4YW1wbGUgT3Jn\n\
YW5pemF0aW9uMRYwFAYDVQQDDA1BbGljZSBFeGFtcGxlMIIBIjANBgkqhkiG9w0B\n\
AQEFAAOCAQ8AMIIBCgKCAQEA3FAEgX6m4lX3cl/jqemZY8h5WmdM8WO5U+ylg6vn\n\
1fE8QbSpIVfaNgPq5DL7yA3t3U2PusPwCZgTEBZ7KPolJlr/Yi6wFxZqnOqqsnDX\n\
SGXmhbu0cvCIUGWD1qRnkeQEx7OQsa5QBUdpwXAqDZC71rbjSd6ihPx5ZAkvbYRq\n\
91fowf7OVCRvxTbQrpcsfLDuXdHUuxLKb4Y06cnhgUamflnHJsxg04wjorY/olJ4\n\
ybEt2EE7PJoic9r4YpIX1Db0cX5fKAO4kyUOZnkbnSTnkkl/dYDEA9++7iv5XCCu\n\
OWPtWOxP2kfidqdpsHX0ZYjoUzO+t1fWohI0Jtbn2YUhbQIDAQABMA0GCSqGSIb3\n\
DQEBCwUAA4IBAQBzl4VJRAPKe5Tk1XgpapcUyE0sLkSGL8wluv2HUSburtFJvq5i\n\
OAqaKc27TVbSjiY5rcSVpXILtqcyHZ9v6quw56wBUNPxDRKOjD3kmJAz/NpAkibk\n\
67u8ysaV9qug9KIlaM6ATNI+2m+xxObhRj6bvoPTcuApPRxr6lUcByiieCdvcCL4\n\
WbVFjjQz49JWLbUHJpRpS+Yq/YfVwAfYxCDi3pzNu3fUvPCczvza2H61uEw3gkXP\n\
ktSJTYAxcN28LQOdqUyqMLB+DRZ7srukJ3AES5nLox2M71Q6OALCit/oc6AOIejC\n\
w83lv/vwDHtbncmREqGorOwGhWypsagl+wNO\n\
-----END CERTIFICATE-----"

#define TEST_REVOKED_IDENTITY_PRIVATE_KEY "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpQIBAAKCAQEA3FAEgX6m4lX3cl/jqemZY8h5WmdM8WO5U+ylg6vn1fE8QbSp\n\
IVfaNgPq5DL7yA3t3U2PusPwCZgTEBZ7KPolJlr/Yi6wFxZqnOqqsnDXSGXmhbu0\n\
cvCIUGWD1qRnkeQEx7OQsa5QBUdpwXAqDZC71rbjSd6ihPx5ZAkvbYRq91fowf7O\n\
VCRvxTbQrpcsfLDuXdHUuxLKb4Y06cnhgUamflnHJsxg04wjorY/olJ4ybEt2EE7\n\
PJoic9r4YpIX1Db0cX5fKAO4kyUOZnkbnSTnkkl/dYDEA9++7iv5XCCuOWPtWOxP\n\
2kfidqdpsHX0ZYjoUzO+t1fWohI0Jtbn2YUhbQIDAQABAoIBAQCcvYkXICZW7NZp\n\
VvNAFWP968j6mnfRXSOjI7/1173PJhu6m2+gu0ISH+Njiyo6gD50rhPNykziZoFZ\n\
dsUUuDLqAN+k2JaLNnWPQh1DaqifZ3AEQTD2fU5d9HtBoCHXV6RW99e/scZYmyAQ\n\
cV5Z3FjnP6KhEXYKqx0qIKbPgeAgK+aX4Xz56vSaq7b4XL0hKMhm/gdvjfLlIawm\n\
HDo8amWFVRhu/JOhCP8CbVAlD7NzSUSHzzVqtDKuuIsYNp2iN+oe0AQVJXDBjaNZ\n\
1c+H+lZ8P0aPhMBGSOdFvf7crwYukFiNgNrRjRIyBqqhwDW6ZZ+rEthQL1kmEcVO\n\
ZuEBkJUBAoGBAPpCo0PIKGXGmeeyqSbBaTGqrN+t/+Gd+DHMxxC6/V/EYRACyQx4\n\
4HY3Be9cCvSO78z/mkf1fEH+vcQ86k878nFY++UWzjAlq4f8GsMmBHB4zzy9Niif\n\
XlCQTKYDu3Y+h7D3Q8W4TLIqjlEiyJnnZN8WWOkBybG0wlQuIk+YkuLhAoGBAOFd\n\
i/DTvBgE9UxRWkD/Hw4PKaptLtTncp5XhtPCAtsBHTIPv11fKI2XBJm/D2lrYuNI\n\
nadVbVPiemOznoylG11Lj0D4e4a9+Wegr2ZYsv49TIU0maCdJmxMeLRvgAHvB+ay\n\
lKEXfrWfXLlpac0euv3TGuEc4JfeSPMFyGWQHxwNAoGBAId98JrQEV8Y6VaSWhZL\n\
fMKRH0tzDyh9uFRuBBDAzFE/JzXd7C++efhGzgXLlXrWsGoSsNrow7+PRfqq6EjB\n\
sf9AKBDeCf/zRS04htzFBn5GSh0ea+YOcqe3mGgBeUsJi7l6Bc1UfOGxPKAc7vK0\n\
Xt6RYM22VBbMQLIG0Di76DrBAoGBANvzCtNeMuNWY3m6pFVvKQX1snqM2PodcXYs\n\
goBFh7fq3G2xhNlCODgIPgs3t3jxv6+HfaaE75DBJyYLdiBaO9zQE94bJaQZ4UJM\n\
RyOiSf9sIDSZY56oAYoNEHk5oTtB6Po1LG4Umiv0fvDOet4gsetsj31JS8GsxpG5\n\
AR5ujI45AoGAK4rfUK58NUXiw3z+wv2BwJRt6A7QSmPUNOsisaiTE483weegwdR3\n\
/lNCfqVCVXQ9I6NXJel/Zd6uBd+TEuJDG72lCbX3fWyoWbfUkghaqSuQow8AsQ+u\n\
OFb1rUwgYVjsO1N15TOsuORn2njfEuxKaHC13FjsjUBq1vFMJU7b4Ho=\n\
-----END RSA PRIVATE KEY-----"

#define TEST_CRL "data:,-----BEGIN X509 CRL-----\n\
MIICRDCCASwCAQEwDQYJKoZIhvcNAQELBQAwgcAxCzAJBgNVBAYTAk5MMQswCQYD\n\
VQQIDAJPVjEWMBQGA1UEBwwNTG9jYWxpdHkgTmFtZTETMBEGA1UECwwKRXhhbXBs\n\
ZSBPVTEjMCEGA1UECgwaRXhhbXBsZSBJRCBDQSBPcmdhbml6YXRpb24xFjAUBgNV\n\
BAMMDUV4YW1wbGUgSUQgQ0ExOjA4BgkqhkiG9w0BCQEWK2F1dGhvcml0eUBjeWNs\n\
b25lZGRzc2VjdXJpdHkuYWRsaW5rdGVjaC5jb20XDTIxMDYxNDE4NTIyN1oXDTIx\n\
MDcxNDE4NTIyN1owJzAlAhQ27u19L06OjJ5TeMyJOGYzN1dDxhcNMjEwNjE0MTg1\n\
MTU5WqAOMAwwCgYDVR0UBAMCAQAwDQYJKoZIhvcNAQELBQADggEBAFTtAIa1CBt5\n\
vquLBsu+pYh2HU2OhD8txOQgHyhSHOjtOymDfM52jAXqe6AO7xg+/WJGtFgrRpLo\n\
tfjNdRwTKVxUk+hp5CqyVwyFu+rKcXs4lQ6NuYpCLplJlX3IurZWYqsUtfwfZPYl\n\
7tFW/u64hgFajHqjWcHNgW4r1XdPiAvmpnY0vwJc/181YQoFzCV3vd4HZ5qxFa1y\n\
7A36+cpAfIYAeqi86mW05PvATZGe3hIRdfdW5khwmqJBQ8h14kWfsc9Sj58R9QSf\n\
lNPzfEqQ0dgtYWokCm3fVYPyfcCc4JQY2ueslDqQR/W66WrHownfnpI59VKt4LC9\n\
wYmF1fVUnoM=\n\
-----END X509 CRL-----"

#endif /* PLUGIN_SECURITY_CORE_TEST_IDENTITY_H_ */
