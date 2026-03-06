.. include:: ../external-links.part.rst

.. index::
    single: PKCS#11
    single: HSM
    single: Hardware Security Module
    single: Security; PKCS#11
    single: Security; HSM

.. _pkcs11_hsm:

##########################################
PKCS#11 and Hardware Security Module (HSM)
##########################################

|var-project-short| supports loading cryptographic credentials from PKCS#11-compatible
devices, including Hardware Security Modules (HSMs). This enables deployments where
private keys never leave the hardware device, providing stronger protection against
key extraction.

PKCS#11 is a platform-independent API (also known as "Cryptoki") originally developed
by RSA Security for interfacing with cryptographic tokens. The standard defines how
applications communicate with devices that store cryptographic objects such as
certificates, public keys, and private keys.

.. contents::
    :local:
    :depth: 2

***************************
Supported security settings
***************************

The following security configuration properties accept ``pkcs11:`` URIs:

.. list-table::
    :header-rows: 1
    :widths: 40 60

    * - Property
      - Description
    * - ``IdentityCertificate``
      - Participant Identity certificate
    * - ``IdentityCA``
      - Participant Identity Certificate Authority certificate
    * - ``PrivateKey``
      - Participant Private Key corresponding to it's identity certificate
    * - ``PermissionsCA``
      - Permissions Certificate Authority certificate

The following properties do **not** support PKCS#11 and must use ``file:`` or ``data:,`` URIs:

- ``Governance``
- ``Permissions``

Governance and Permissions documents are signed XML files. Their integrity is protected
cryptographically via PKCS#7 signatures, verified using the Permissions CA certificate
(which can be stored on an HSM).

**************
URI format
**************

PKCS#11 URIs follow :rfc:`7512` and identify objects stored on a token. The general format is::

    pkcs11:[path-attr[;path-attr...]][?query-attr[&query-attr...]]

Common path components:

.. list-table::
    :header-rows: 1
    :widths: 25 75

    * - Component
      - Description
    * - ``token``
      - Label of the token
    * - ``object``
      - Label of the object (certificate or key)
    * - ``type``
      - Object type: ``cert``, ``public``, ``private``, or ``secret-key``
    * - ``pin-value``
      - PIN for token login

.. note::
    The ``pin-source`` attribute defined in :rfc:`7512` is not currently supported by
    |var-project-short|. The PIN must be specified using ``pin-value`` in the URI.

Examples::

    pkcs11:token=MyHSM;object=identity_cert
    pkcs11:token=MyHSM;object=identity_key;type=private
    pkcs11:token=MyHSM;object=identity_key?pin-value=1234

*************
Prerequisites
*************

OpenSSL version differences
===========================

The mechanism for PKCS#11 integration differs between OpenSSL versions:

OpenSSL 1.x (ENGINE API)
------------------------

OpenSSL 1.x uses the deprecated ENGINE API. Install the ``libp11`` engine package for your
distribution (for example, ``libengine-pkcs11-openssl`` on Debian/Ubuntu or ``openssl-pkcs11``
on Fedora/RHEL).

Configure OpenSSL to use the engine by adding it to ``/etc/ssl/openssl.cnf`` (or equivalent):

.. code-block:: ini

    openssl_conf = openssl_init

    [openssl_init]
    engines = engine_section

    [engine_section]
    pkcs11 = pkcs11_section

    [pkcs11_section]
    engine_id = pkcs11
    MODULE_PATH = /usr/lib/pkcs11/opensc-pkcs11.so
    init = 0

Adjust ``MODULE_PATH`` to point to your PKCS#11 module.

OpenSSL 3.x (Provider API)
--------------------------

OpenSSL 3.x uses the Provider architecture. Install a provider
and configure openssl to use it.

Configure the provider in ``/etc/ssl/openssl.cnf``:

.. code-block:: ini

    openssl_conf = openssl_init

    [openssl_init]
    providers = provider_section

    [provider_section]
    default = default_section
    pkcs11 = pkcs11_section

    [default_section]
    activate = 1

    [pkcs11_section]
    module = /usr/lib/ossl-modules/pkcs11.so
    pkcs11-module-path = /usr/lib/pkcs11/opensc-pkcs11.so
    activate = 1

PKCS#11 module
==============

A PKCS#11 module (shared library) is required to communicate with the token. Common options are:

- **OpenSC** (``opensc-pkcs11.so``): Smart cards and USB tokens
- **SoftHSM** (``libsofthsm2.so``): Software-based HSM for testing
- **Vendor modules**: Provided by HSM vendors (Thales, Utimaco, etc.)

*********************
Example configuration
*********************

The following configuration uses PKCS#11 for the private key and identity certificate
while keeping policy documents on the filesystem:

.. code-block:: xml

    <?xml version="1.0" encoding="UTF-8" ?>
    <CycloneDDS xmlns="https://cdds.io/config">
        <Domain id="any">
            <Security>
                <Authentication>
                    <IdentityCertificate>pkcs11:token=DDSTestToken;object=alice_cert?pin-value=1234</IdentityCertificate>
                    <IdentityCA>file:identity_ca_cert.pem</IdentityCA>
                    <PrivateKey>pkcs11:token=DDSTestToken;object=alice_key;type=private?pin-value=1234</PrivateKey>
                </Authentication>
                <AccessControl>
                    <PermissionsCA>pkcs11:token=DDSTestToken;object=permissions_ca?pin-value=1234</PermissionsCA>
                    <Governance>file:governance.p7s</Governance>
                    <Permissions>file:permissions.p7s</Permissions>
                </AccessControl>
            </Security>
        </Domain>
    </CycloneDDS>

.. warning::
    The PIN is embedded directly in the configuration when using ``pin-value``. For
    production deployments, restrict access to the configuration file and consider
    using environment variable substitution or other mechanisms to avoid storing
    PINs in plain text.

***************
Troubleshooting
***************

Verifying OpenSSL PKCS#11 support
=================================

**OpenSSL 1.x:**

.. code-block:: console

    openssl engine pkcs11 -t

Expected output includes "available".

**OpenSSL 3.x:**

.. code-block:: console

    openssl list -providers

The pkcs11 provider should be listed.

Testing certificate access
==========================

**OpenSSL 3.x:** Verify that OpenSSL can load a certificate from the token using its URI
(requires ``pkcs11-provider`` configured in ``openssl.cnf``):

.. code-block:: console

    openssl x509 -in "pkcs11:token=DDSTestToken;object=alice_cert?pin-value=1234" -text -noout

**OpenSSL 1.x:** The ENGINE API does not support loading certificates via the ``openssl x509``
command. Use ``pkcs11-tool`` to verify the certificate is present on the token instead:

.. code-block:: console

    pkcs11-tool --module /usr/lib/softhsm/libsofthsm2.so \
        --token-label "DDSTestToken" --pin 1234 --list-objects --type cert

Common errors
=============

**"Failed to find pkcs11 engine"** (OpenSSL 1.x)

The PKCS#11 engine is not installed or not configured in ``openssl.cnf``. Verify the
engine is available with ``openssl engine -t pkcs11``.

**"Failed to open OSSL_STORE"** (OpenSSL 3.x)

The PKCS#11 provider is not installed or configured. Verify provider
configuration (``openssl list -providers``) and ensure the
``pkcs11-module-path`` points to a valid PKCS#11 module.

**"CKR_PIN_INCORRECT"**

The PIN is incorrect. Verify the PIN and check if the token is locked due to
too many failed attempts.

**"CKR_TOKEN_NOT_PRESENT"**

The token is not connected or recognized. Check the connection and verify
the PKCS#11 module path is correct.

**"Failed to find certificate"**

The object label does not match any certificate on the token. Use ``pkcs11-tool --list-objects``
to verify object labels.

***********************
Security considerations
***********************

- The PIN must currently be specified via ``pin-value`` in the URI. Restrict access to
  configuration files containing PINs (e.g., mode 0400) and consider using configuration
  management tools that support secret injection.

- Ensure the PKCS#11 module is from a trusted source. A malicious module has full access
  to cryptographic operations.

- For HSMs, configure appropriate policies for key usage, including restrictions on
  export and the operations permitted with each key.

- Consider the security of the system where |var-project-short| runs. An attacker with
  sufficient access to the host can still perform cryptographic operations using
  the HSM, even without extracting the key.
