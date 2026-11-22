Entities
========

All entities are represented by a process-private handle, with one call to enable an entity when it was created disabled.
An entity is created enabled by default. Note: disabled creation is currently not supported.

.. doxygengroup:: entity
    :project: ddsc_api_docs
    :members:

.. doxygengroup:: entity_qos
    :project: ddsc_api_docs
    :members:

DCPS Status
-----------

.. doxygengroup:: dcps_status_getters
    :project: ddsc_api_docs
    :members:

Interaction with Listeners
--------------------------

You can find the documentation on the interactions with listeners :ref:`in the listener API documentation <section_entity_listener>`:.
