.. include:: ../external-links.part.rst

.. index:: 
    single: Proprietary builtin endpoints
    single: DDS security; Endpoints
    single: Security; Endpoints


Proprietary builtin endpoints
*****************************

The DDS standard contains some builtin endpoints. A few are added by the DDS Security
specification (|url::omg.security|). The behaviour of all these builtin endpoints are 
specified and handled by the plugins (see :ref:`Plugins_configuration`) that implement 
the OMG DDS Security specification. That is, they do not have to be present in the 
:ref:`governance document <governance_document>` or :ref:`permissions document 
<permissions_document>`, and users do not have to consider these endpoints.

A few of these builtin endpoints behave according to the *protection kinds* within
the :ref:`governance document <governance_document>`. Related submessages are protected 
according to the value of their protection kind, which protects the meta information that 
is send during the discovery phase.