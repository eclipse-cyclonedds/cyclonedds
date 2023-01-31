.. include:: ../external-links.part.rst

.. index:: 
  single: Plugin; Configuration
  single: DDS security; Plugin configuration
  single: Security; Plugin configuration

.. _`Plugins_configuration`:

Plugin configuration
********************

|var-project-short| gets the security configuration from XML configuration elements or from
the participant QoS policies as stated in the OMG DDS Security specification (|url::omg.security|).

This behavior allows applications to use DDS Security without recompiling the binaries.
Supplying a new configuration with DDS Security enabled is enough to switch from a
non-secure to a secure deployment. The configuration is at domain level, which means
that all participants created for that domain receive the same DDS security settings.

The configuration options for a domain are in the |var-project-short| configuration 
(:ref:`/Domain/Security <//CycloneDDS/Domain/Security>`). Every DDS Security plugin has its 
own configuration sub-section.

.. index:: Authentication properties

.. _`Authentication Properties`:

=========================
Authentication properties
=========================

To enable authentication for a node, it must be configured with an 
:ref:`IdentityCertificate <//CycloneDDS/Domain/Security/Authentication/IdentityCertificate>`, 
which authenticates all participants of that particular |var-project-short| domain. Associated 
with the identity certificate is the corresponding 
:ref:`PrivateKey <//CycloneDDS/Domain/Security/Authentication/PrivateKey>`. 

The private key is either a 2048-bit RSA key, or a 256-bit Elliptic Curve Key with a prime256v1 curve.

The certificate of identity CA, which is the issuer of the node's identity certificate,
is configured in :ref:`IdentityCA <//CycloneDDS/Domain/Security/Authentication/IdentityCA>`. 

The public key of the identity CA (as part of its certificate) is either a 2048-bit RSA key, 
or a 256-bit Elliptic Curve key for the prime256v1 curve. The identity CA certificate can be 
a self-signed certificate.

The identity certificate, private key and the identity CA should be a X509 document in PEM
format. It may either be specified directly in the configuration file (as CDATA, prefixed
with ``data:,``), or the configuration file should contain a reference to a corresponding
file (prefixed with ``file:``).

Optionally, the private key can be protected by a
:ref:`password <//CycloneDDS/Domain/Security/Authentication/Password>`.

To enable multiple identity CAs throughout the system, you can configure a directory that 
contains additional identity CA's that verify the identity certificates received from remote 
instances
(:ref:`TrustedCADirectory <//CycloneDDS/Domain/Security/Authentication/TrustedCADirectory>`).

.. index:: 
  single: Plugin; Access control
  single: Access control; Properties
  single: Governance document
  single: Permissions document
  single: Permissions certificate

.. _`Access Control Properties`:

=========================
Access control properties
=========================

The following are are required for the access control plugin:

- A governance document (:ref:`//CycloneDDS/Domain/Security/AccessControl/Governance`).
- a permissions document (:ref:`//CycloneDDS/Domain/Security/AccessControl/Permissions`). 
- The permissions CA certificate (:ref:`//CycloneDDS/Domain/Security/AccessControl/PermissionsCA`).

These values can be provided as CDATA or by using a path to a file (Similar to the authentication plugin properties).

.. index:: Cryptography

.. _`Cryptography Properties`:

=======================
Cryptography properties
=======================

The cryptography plugin has no configuration properties.
