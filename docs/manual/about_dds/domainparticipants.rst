.. index:: DomainParticipants

.. _domainparticipants_bm:

==================
DomainParticipants
==================

A Domain is a specific subsection of the DDS shared-dataspace and identified by its 
domain ID, which is a 32-bit unsigned integer.

Data exchanges are limited to the domain they are made on. For example, data exchanged 
on domain 456 is not visible on domain 789.

To exchange data you must create a DomainParticipant, which is an entrypoint for the 
program on the shared dataspace's domain.

To specify the default domain ID:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++
			
			dds::domain::DomainParticipant participant(domain::default_id());

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

To specify your own ID:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++
			
			dds::domain::DomainParticipant participant(123456);

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

.. important::
	You must have the same ID on both the reading side and the writing side, otherwise, 
	they can not see each other.

.. tbd:: 
	???Explain more about setting QoSes???

