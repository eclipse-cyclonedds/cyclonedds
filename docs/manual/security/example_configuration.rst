.. include:: ../external-links.part.rst

.. index::
    single: Security; Example configuration
    single: DDS security; Example configuration
    single: Access control
    single: Examples; Example DDS security configuration

.. _Example_security_configuration:

**********************************
Example DDS security configuration
**********************************

This section shows an example |var-project-short| configuration for DDS Security. 

The steps for configuring DDS Security are:

#. :ref:`create_certificate_authority` 
#. :ref:`create_identity_certificate`
#. :ref:`create_governance_document`
#. :ref:`create_permissions_document`
#. Either:

  - :ref:`set_security_properties`
  - :ref:`apply_security_settings`

.. index:: Certificate authority; Creating

.. _create_certificate_authority:

===============================================
Create a permissions Certificate Authority (CA)
===============================================

To generate the CA for identity management (authentication):

#. Create the private key for the CA:

   .. code-block:: console

     openssl genrsa -out example_id_ca_priv_key.pem 2048

#. Create the certificate for the identity CA (which is a self-signed certificate):

   .. code-block:: console

     openssl req -x509 -key example_id_ca_priv_key.pem -out example_id_ca_cert.pem -days 3650 -subj "/C=NL/ST=OV/L=Locality Name/OU=Example OU/O=Example ID CA Organization/CN=Example ID CA/emailAddress=authority@cycloneddssecurity.zettascale.com"

#. Create the private key of the permissions CA (used for signing the AccessControl 
   configuration files):

   .. code-block:: console

     openssl genrsa -out example_perm_ca_priv_key.pem 2048

#. Create the self-signed certificate for the permissions CA:

   .. code-block:: console

     openssl req -x509 -key example_perm_ca_priv_key.pem -out example_perm_ca_cert.pem -days 3650 -subj "/C=NL/ST=OV/L=Locality Name/OU=Example OU/O=Example CA Organization/CN=Example Permissions CA/emailAddress=authority@cycloneddssecurity.zettascale.com"

.. index:: Identity certificate; Creating

.. _create_identity_certificate:

==============================
Create an identity certificate
==============================

Create an identity certificate (signed by the CA), and the private key corresponding to an identity named *Alice*.

.. note:: 
  These steps need to be repeated for each identity in the system.

To create a private key and an identity certificate for an identity named *Alice*:

#. Create the **private key** for *Alice's* identity:

   .. code-block:: console

     openssl genrsa -out example_alice_priv_key.pem 2048

#. To request that the identity CA generates a certificate, create a Certificate Signing Request (:term:`CSR`):

   .. code-block:: console

     openssl req -new -key example_alice_priv_key.pem -out example_alice.csr -subj "/C=NL/ST=OV/L=Locality Name/OU=Organizational Unit Name/O=Example Organization/CN=Alice Example/emailAddress=alice@cycloneddssecurity.zettascale.com"

#. Create *Alice's* **identity certificate**:

   .. code-block:: console

     openssl x509 -req -CA example_id_ca_cert.pem -CAkey example_id_ca_priv_key.pem -CAcreateserial -days 3650 -in example_alice.csr -out example_alice_cert.pem

#. In the DDS Security authentication configuration:

   - Use *Alice's* private key (example_alice_priv_key.pem) file for the 
     :ref:`PrivateKey <//CycloneDDS/Domain/Security/Authentication/PrivateKey>` setting. 

   - Use *Alice's* identity certificate (example_alice_cert.pem) file for the
     :ref:`IdentityCertificate <//CycloneDDS/Domain/Security/Authentication/IdentityCertificate>` setting. 
 
   - Use the certificate of the CA used for signing this identity (example_id_ca_cert.pem), 
     for the :ref:`IdentityCA <//CycloneDDS/Domain/Security/Authentication/IdentityCA>` setting.

.. index:: Governance document; Creating

.. _create_governance_document:

===================================
Create a signed governance document
===================================

The following shows an example of a governance document that uses *signing for submessage* and an encrypted payload:

.. literalinclude:: ../_static/example_governance.xml
    :linenos:
    :language: xml

The governance document must be signed by the :ref:`permissions CA <create_certificate_authority>`. 

To sign the governance document:

.. code-block:: console

  openssl smime -sign -in example_governance.xml -text -out example_governance.p7s -signer example_perm_ca_cert.pem -inkey example_perm_ca_priv_key.pem

.. index:: Permissions document; Creating

.. _create_permissions_document:

====================================
Create a signed permissions document
====================================

The permissions document is an XML document that contains the permissions of the participant and
binds them to the subject name in the identity certificate (distinguished name) for the participant
as defined in the DDS |url::DDS_plugins_authentication|.

An example of a permissions document:

.. literalinclude:: ../_static/example_permissions.xml
    :linenos:
    :language: xml

This document also needs to be signed by the :ref:`permissions CA <create_certificate_authority>`:

.. code-block:: console

  openssl smime -sign -in example_permissions.xml -text -out example_permissions.p7s -signer example_perm_ca_cert.pem -inkey example_perm_ca_priv_key.pem


.. _set_security_properties:

==============================================
Set the security properties in participant QoS
==============================================

The following code fragment shows how to set the security properties to a QoS object, and 
use this QoS when creating a participant:

.. literalinclude:: ../_static/security_by_qos.c
    :linenos:
    :language: c

.. _apply_security_settings:

=======================
Apply security settings
=======================

As an alternative for using the QoS, security settings can also be applied using the |var-project-short|
configuration XML. If both QoS and the configuration XML contain security settings, the values
from the QoS is used and the security settings in the configuration XML are ignored.

The following XML fragment shows how to set security settings through configuration:

.. literalinclude:: ../_static/security_by_config.xml
    :linenos:
    :language: xml

To use this configuration file for an application, set the ``CYCLONEDDS_URI`` environment
variable to this config file:

.. code-block:: console

  export CYCLONEDDS_URI=/path/to/secure_config.xml

.. note:: 
  This example configuration uses the attribute ``id=any`` for the ``domain`` element, any participant
  that is created (which implicitly creates a domain) in an application using this configuration gets
  these security settings.
