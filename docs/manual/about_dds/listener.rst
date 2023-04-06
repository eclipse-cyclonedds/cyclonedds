..
   Copyright(c) 2022 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. index:: ! Listeners

.. _listeners_bm:

Listeners
=========

Listeners enable the code to react to changes in state of DDS entities such as 
readers, writers, and so on. |var-project| implements different listeners for 
different entities. Some types' listeners inherit from other types' listeners, 
allowing the listener of one type to react to changes in state on subordinate entities.
For example, by implementing the required callback functions for :ref:`topics <topics_bm>` 
and :ref:`subscribers <subscribers_bm>` in a ``DomainParticipantListener``, this listener 
only needs to be set once in the :ref:`DomainParticipant <domainparticipants_bm>`, and 
the topic and subscriber propagates the events to this Listener.

.. table:: Entities and associated listeners

	+-----------------------+---------------------------+-----------------------+-------------------------------+----------------------------+--------------------------------+
	| Entity Type           | Listener Type             | Inherits From         | Associated Unique Callbacks   | Associated StatusMask      | Passed Status Entity           |
	+=======================+===========================+=======================+===============================+============================+================================+
	| DomainParticipant     | DomainParticipantListener | PublisherListener     |                               |                            |                                |
	|                       |                           | SubscriberListener    |                               |                            |                                |
	|                       |                           | AnyTopicListener      |                               |                            |                                |
	+-----------------------+---------------------------+-----------------------+-------------------------------+----------------------------+--------------------------------+
	| Topic                 | TopicListener             |                       | on_inconsistent_topic         | inconsistent_topic         | InconsistentTopicStatus        |
	+-----------------------+---------------------------+-----------------------+-------------------------------+----------------------------+--------------------------------+
	| Publisher             | PublisherListener         | DataWriterListener    |                               |                            |                                |
	+-----------------------+---------------------------+-----------------------+-------------------------------+----------------------------+--------------------------------+
	| DataWriter            | DataWriterListener        |                       | on_offered_deadline_missed    | offered_deadline_missed    | OfferedDeadlineMissedStatus    |
	|                       |                           |                       +-------------------------------+----------------------------+--------------------------------+
	|                       |                           |                       | on_offered_incompatible_qos   | offered_incompatible_qos   | OfferedIncompatibleQosStatus   |
	|                       |                           |                       +-------------------------------+----------------------------+--------------------------------+
	|                       |                           |                       | on_liveliness_lost            | liveliness_lost            | LivelinessLostStatus           |
	|                       |                           |                       +-------------------------------+----------------------------+--------------------------------+
	|                       |                           |                       | on_publication_matched        | publication_matched        | PublicationMatchedStatus       |
	+-----------------------+---------------------------+-----------------------+-------------------------------+----------------------------+--------------------------------+
	| Subscriber            | SubscriberListener        | DataReaderListener    |                               |                            |                                |
	+-----------------------+---------------------------+-----------------------+-------------------------------+----------------------------+--------------------------------+
	| DataReader            | DataReaderListener        |                       | on_requested_deadline_missed  | requested_deadline_missed  | RequestedDeadlineMissedStatus  |
	|                       |                           |                       +-------------------------------+----------------------------+--------------------------------+
	|                       |                           |                       | on_requested_incompatible_qos | requested_incompatible_qos | RequestedIncompatibleQosStatus |
	|                       |                           |                       +-------------------------------+----------------------------+--------------------------------+
	|                       |                           |                       | on_sample_rejected            | sample_rejected            | SampleRejectedStatus           |
	|                       |                           |                       +-------------------------------+----------------------------+--------------------------------+
	|                       |                           |                       | on_liveliness_changed         | liveliness_changed         | LivelinessChangedStatus        |
	|                       |                           |                       +-------------------------------+----------------------------+--------------------------------+
	|                       |                           |                       | on_data_available             | data_available             |                                |
	|                       |                           |                       +-------------------------------+----------------------------+--------------------------------+
	|                       |                           |                       | on_subscription_matched       | subscription_matched       | SubscriptionMatchedStatus      |
	|                       |                           |                       +-------------------------------+----------------------------+--------------------------------+
	|                       |                           |                       | on_sample_lost                | sample_lost                | SampleLostStatus               |
	+-----------------------+---------------------------+-----------------------+-------------------------------+----------------------------+--------------------------------+

A Listener is implemented as a virtual base class that defines a number of functions that correspond to status transitions in the underlying entity.
For example, the DataReaderListener has an unimplemented virtual on_data_available function, which will be called each time data is inserted into the associated DataReader's history.
DDS entities can be passed to a listener during creation, together with a StatusMask that describes which status changes to pass to the listener.

To make use of the listener functionality, create a class deriving from the type of listener necessary and implement the virtual functions.
To simplify the use of listeners, there are also NoOp listeners that implements all virtual functions with empty contents, which enables you to implement the functions you are interested in.

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			template<typename T>
			ExampleListener: public dds::sub::NoOpDataReaderListener<T> {
				public:
					void on_data_available(dds::sub::DataReader<T>& reader) {
						/* you also get a reference to the reader the callback originated from */
						auto samples = reader.take();
						std::cout << "I have " << samples.length() << " new samples available." << std::endl;
						int invsamples = 0;
						for (const auto & sample:samples) {
							if (!sample.info().valid())
								invsamples++;
						}
						std::cout << "Of which " << invsamples << " were invalid samples." << std::endl;
					}
			};

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD


By passing this listener to a reader and setting the correct status mask, the message "I have $N new samples available.", and then "Of which $I were invalid samples.", where $N is the number of new samples and $I the number of invalid samples, appears each time the associated reader receives data:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			dds::sub::qos::DataReaderQos drqos;
			ExampleListener<DataType> listener;
			dds::sub::DataReader<DataType> reader(subscriber, topic, drqos, &listener, dds::core::status::StatusMask::data_available());

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

Some listeners' callback functions pass references to the entities that the callback originated from and/or status objects and contain information relevant to the status change.
For example, the listener for DataWriters has the following callback function that is triggered when the Deadline QoS Policy is not complied with:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++

			void offered_deadline_missed(dds::pub::AnyDataWriter& writer, const dds::core::status::OfferedDeadlineMissedStatus& status);

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

