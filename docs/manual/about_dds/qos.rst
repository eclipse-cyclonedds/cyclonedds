..
   Copyright(c) 2022 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. include:: ../external-links.part.rst
.. index:: ! Quality of service

.. _qos_bm:

Quality of Service
==================

Quality of Service is the collection of restrictions and expectations (called 
QoSPolicies) that appy to the different components of CycloneDDS. Some QoSPolicies 
only affect a single type of DDS entity, whereas others affect multiple DDS entities. 

.. important:: 
	QoSPolicies on a DDS entity **can not** be modified after creating the entity as they 
	affect matching/discovery of entities. 

The QoS used is linked to the type of DDS entity the QoS is used for. For example, a 
SubscriberQoS is not accepted when creating a DataReader. 

- The entity-specific QoSes are found in the `qos` sub-namespace of the same namespace 
  the entity is defined in.

- The different QoSPolicies are located in the `dds::core::policy` namespace.

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. table:: DDS entity types and associated QoSes

			+-----------------------+----------------------+
			| CDDS-CXX Entity Type  | QoS Type             |
			+=======================+======================+
			| DomainParticipant     | DomainParticipantQos |
			+-----------------------+----------------------+
			| Publisher             | PublisherQos         |
			+-----------------------+----------------------+
			| Subscriber            | SubscriberQos        |
			+-----------------------+----------------------+
			| Topic                 | TopicQos             |
			+-----------------------+----------------------+
			| DataWriter            | DataWriterQos        |
			+-----------------------+----------------------+
			| DataReader            | DataReaderQos        |
			+-----------------------+----------------------+

		.. table:: QoSPolicies and associated QoSes:

			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| QoSPolicy                  | DomainParticipantQos | PublisherQos | SubscriberQos | TopicQos | DataWriterQos | DataReaderQos |
			+============================+======================+==============+===============+==========+===============+===============+
			| UserData                   | Y                    | N            | N             | N        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| EntityFactory              | Y                    | Y            | Y             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| TopicData                  | N                    | N            | N             | Y        | N             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| Durability                 | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| DurabilityService          | N                    | N            | N             | Y        | Y             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| Deadline                   | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| LatencyBudget              | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| Liveliness                 | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| Reliability                | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| DestinationOrder           | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| History                    | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| ResourceLimits             | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| TransportPriority          | N                    | N            | N             | Y        | Y             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| Lifespan                   | N                    | N            | N             | Y        | Y             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| Ownership                  | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| Presentation               | N                    | Y            | Y             | N        | N             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| Partition                  | N                    | Y            | Y             | N        | N             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| GroupData                  | N                    | Y            | Y             | N        | N             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| OwnershipStrength          | N                    | N            | N             | N        | Y             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| WriterDataLifecycle        | N                    | N            | N             | N        | Y             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| TimeBasedFilter            | N                    | N            | N             | N        | N             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| ReaderDataLifecycle        | N                    | N            | N             | N        | Y             | N             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| DataRepresentation         | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+
			| TypeConsistencyEnforcement | N                    | N            | N             | Y        | Y             | Y             |
			+----------------------------+----------------------+--------------+---------------+----------+---------------+---------------+


    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD


Setting of QoSPolicies can be done by:

Either left-shifting the QoSPolicy "into" the QoS:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			dds::sub::qos::DataReaderQos rqos;
			rqos << dds::core::policy::Durability(dds::core::policy::DurabilityKind::TRANSIENT_LOCAL);

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD


Or passing it as the parameter of the `policy` function:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			dds::pub::qos::DataWriterQos wqos;
			dds::core::policy::Reliability rel(dds::core::policy::ReliabilityKind::RELIABLE, dds::core::Duration(8, 8));
			wqos.policy(rel);

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD


Getting of QoSPolicies can be done by:

Either through the right-shifting the QoSPolicy "out of" the QoS:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			dds::topic::qos::TopicQos tqos;
			dds::core::policy::TopicData td;
			tqos >> td;

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD


Or through the `policy` function, which is templated to indicate which QoSPolicy is 
being accessed:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			dds::domain::qos::DomainParticipantQos dqos;
			auto ud = dqos.policy<dds::core::policy::UserData>();

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD


For a detailed explanation of the different QoSPolicies and their effects on the 
behaviour of CycloneDDS, refer to the |url::dds_spec| v1.4 section 2.2.3.

Default and Inherited QoSes
---------------------------

QoSes have a number of default settings that are falled-back to when none are provided 
on creation. These defaults are either defined in the DDS standard, or propagated from 
"superior" entities. The default inherited QoS for entities is set through the 
following functions:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. table:: Default QoSes and accessors

			+-------------------+--------------------+------------------------+
			| Superior Entity   | Subordinate Entity | Default QoS accessor   |
			+===================+====================+========================+
			| DomainParticipant | Topic              | default_topic_qos      |
			|                   +--------------------+------------------------+
			|                   | Publisher          | default_publisher_qos  |
			|                   +--------------------+------------------------+
			|                   | Subscriber         | default_subscriber_qos |
			+-------------------+--------------------+------------------------+
			| Topic             | DataReader         | default_datareader_qos |
			|                   +--------------------+------------------------+
			|                   | DataWriter         | default_datawriter_qos |
			+-------------------+--------------------+------------------------+
			| Publisher         | DataWriter         | default_datawriter_qos |
			+-------------------+--------------------+------------------------+
			| Subscriber        | DataReader         | default_datareader_qos |
			+-------------------+--------------------+------------------------+

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

For example, in the following case:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			dds::sub::Subscriber sub(participant);
			dds::sub::qos::DataReaderQos qos1, qos2;
			qos1 << dds::core::policy::Durability(dds::core::policy::DurabilityKind::TRANSIENT_LOCAL);
			qos2 << dds::core::policy::DestinationOrder(dds::core::policy::DestinationOrderKind::BY_SOURCE_TIMESTAMP);
			sub.default_datareader_qos(qos1);
			dds::sub::DataReader<DataType> reader(sub,topic,qos2);

		`reader` has its `DestinationOrder` QoSPolicy set to the value set in the QoS supplied 
		in its constructor, which is `BY_SOURCE_TIMESTAMP`. `Durability` QoSPolicy defaults 
		to the one set as default on the Subscriber, which is `TRANSIENT_LOCAL`. All other 
		QosPolicies default to the DDS Spec, for example, the `Ownership` QoSPolicy has the 
		value `SHARED`.

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

