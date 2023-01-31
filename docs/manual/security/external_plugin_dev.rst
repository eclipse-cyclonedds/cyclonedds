.. include:: ../external-links.part.rst

.. index:: 
  single: External plugin development
  single: Plugin; External development
  single: DDS security; External plugins
  single: Security; External plugins

.. _external_plugin_dev:

***************************
External plugin development
***************************

|var-project-short| has three built-in security plugins that comply with the OMG 
|url::omg.security| specification:

- Authentication
- AccessControl
- Cryptographic

Security plugins are dynamically loaded. The locations are defined in |var-project-short|
configuration or participant QoS settings, see :ref:`DDS_security`.

You can add your own custom plugin in an API by implementing according to the OMG 
|url::omg.security| specification. You can implement all of the plugins or just one of them.

Interface
*********

Implement all plugin-specific functions with exactly same prototype. Plugin-specific function
interfaces are in the following header files:

- *dds_security_api_access_control.h*
- *dds_security_api_authentication.h*
- *dds_security_api_cryptography.h*


``init`` and ``finalize`` functions
***********************************

A plugin must have an ``init`` and a ``finalize`` functions. The ``plugin_init`` and 
``plugin_finalize`` interfaces are found in the *dds_security_api.h* header file. The 
functions must be same as in the configuration file.

- After the plugin is loaded, the ``init`` function is called. 

- Before the plugin is unloaded, the ``finalize`` function is called.

Inter-plugin communication
**************************

Within the authentication and cryptography plugins, there is a shared object (*DDS_Security_SharedSecretHandle*). 

To implement one of the security plugins, and use the built-in for the other one, you must 
get, or provide the shared object:

- *DDS_Security_SharedSecretHandle* is the integer representation of the
  *DDS_Security_SharedSecretHandleImpl* struct object. 

- The cryptography plugin gets the *DDS_Security_SharedSecretHandle* from the authentication 
  plugin and casts to the *DDS_Security_SharedSecretHandleImpl* struct. 

All required information can be retrieved through the *DDS_Security_SharedSecretHandleImpl* 
struct:

::

  typedef struct DDS_Security_SharedSecretHandleImpl {
   DDS_Security_octet* shared_secret;
   DDS_Security_long shared_secret_size;
   DDS_Security_octet challenge1[DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE];
   DDS_Security_octet challenge2[DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE];

  } DDS_Security_SharedSecretHandleImpl;
