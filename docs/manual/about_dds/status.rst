..
   Copyright(c) 2022 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. index:: ! Statuses

.. _statuses_bm:

Statuses
========

Entities in DDS have statuses that indicate the internal state and/or history of an 
entity.These statuses signal Waitsets and Listeners, and are different between types 
of DDS entities. The following table lists the fields for each status and their meaning:

.. table:: CycloneDDS-CXX Status entities and fields

	+-----------------------+--------------------------------+--------------------------+------------------------------------------------------+
	| Entity Type           | Status Entity                  | Associated Fields        | Meaning of field                                     |
	+=======================+================================+==========================+======================================================+
	| DomainParticipant     |                                |                          |                                                      |
	+-----------------------+--------------------------------+--------------------------+------------------------------------------------------+
	| Publisher             |                                |                          |                                                      |
	+-----------------------+--------------------------------+--------------------------+------------------------------------------------------+
	| Subscriber            |                                |                          |                                                      |
	+-----------------------+--------------------------------+--------------------------+------------------------------------------------------+
	| Topic                 | InconsistentTopicStatus        | total_count              | the total number of times an inconsistent topic was  |
	|                       |                                |                          | encountered                                          |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	+-----------------------+--------------------------------+--------------------------+------------------------------------------------------+
	| DataWriter            | OfferedDeadlineMissedStatus    | total_count              | the total number of times an offered deadline was    |
	|                       |                                |                          | missed by the writer committing itself to the        |
	|                       |                                |                          | deadline                                             |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | last_instance_handle     | handle to the last instance in a writer missing a    |
	|                       |                                |                          | committed deadline                                   |
	|                       +--------------------------------+--------------------------+------------------------------------------------------+
	|                       | OfferedIncompatibleQosStatus   | total_count              | the total number of times the writer encountered a   |
	|                       |                                |                          | reader requesting a QoS which was was not compatible |
	|                       |                                |                          | with the one offered by the writer                   |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | last_policy_id           | the last id of the QoSPolicy which was incompatible  |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | policies                 | a collection of QoSPolicy ids and the number times   |
	|                       |                                |                          | they were found to be incompatible                   |
	|                       +--------------------------------+--------------------------+------------------------------------------------------+
	|                       | LivelinessLostStatus           | total_count              | the total number of times a writer became "dead" by  |
	|                       |                                |                          | not asserting its liveliness often enough            |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	|                       +--------------------------------+--------------------------+------------------------------------------------------+
	|                       | PublicationMatchedStatus       | total_count              | the total number of times a writer has encountered a |
	|                       |                                |                          | reader it has a "match" with                         |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | current_count            | the current number of readers a writer has a "match" |
	|                       |                                |                          | with                                                 |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | current_count_change     | the change in current_count since last being         |
	|                       |                                |                          | retrieved                                            |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | last_subscription_handle | handle to the last reader causing this status to     |
	|                       |                                |                          | change                                               |
	+-----------------------+--------------------------------+--------------------------+------------------------------------------------------+
	| DataReader            | RequestedDeadlineMissedStatus  | total_count              | total number of missed deadlines for instances read  |
	|                       |                                |                          | by the reader                                        |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | last_instance_handle     | the last instance handle to miss a deadline          |
	|                       +--------------------------------+--------------------------+------------------------------------------------------+
	|                       | RequestedIncompatibleQosStatus | total_count              | the total number of times the reader encountered a   |
	|                       |                                |                          | writer offering a QoS which was was not compatible   |
	|                       |                                |                          | with the one requested by the reader                 |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | last_policy_id           | the last id of the QoSPolicy which was incompatible  |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | policies                 | a collection of QoSPolicy ids and the number times   |
	|                       |                                |                          | they were found to be incompatible                   |
	|                       +--------------------------------+--------------------------+------------------------------------------------------+
	|                       | SampleRejectedStatus           | total_count              | total number of samples rejected by the reader       |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | last_reason              | the last reason for rejecting a sample               |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | last_instance_handle     | handle to last instance encountering a rejected      |
	|                       |                                |                          | sample                                               |
	|                       +--------------------------------+--------------------------+------------------------------------------------------+
	|                       | LivelinessChangedStatus        | alive_count              | the total number of matching writers that are alive  |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | alive_count_change       | the change in alive_count since last being retrieved |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | not_alive_count          | the total number of matching writers that are dead   |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | not_alive_count_change   | the change in not_alive_count since last being       |
	|                       |                                |                          | retrieved                                            |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | last_publication_handle  | handle to the last writer causing this to change     |
	|                       +--------------------------------+--------------------------+------------------------------------------------------+
	|                       | SubscriptionMatchedStatus      | total_count              | the total number of times a reader has encountered a |
	|                       |                                |                          | writer it has a "match" with                         |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | current_count            | the current number of writers a reader has a "match" |
	|                       |                                |                          | with                                                 |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | current_count_change     | the change in current_count since last being         |
	|                       |                                |                          | retrieved                                            |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | last_publication_handle  | handle to the writer causing this status to change   |
	|                       +--------------------------------+--------------------------+------------------------------------------------------+
	|                       | SampleLostStatus               | total_count              | the total number of samples lost in the topic        |
	|                       |                                +--------------------------+------------------------------------------------------+
	|                       |                                | total_count_change       | the change in total_count since last being retrieved |
	+-----------------------+--------------------------------+--------------------------+------------------------------------------------------+

Fields of status objects containing counts of events will have both a cumulative and interval count,
where the cumulative count will keep track of all changes during the lifetime of the DDS entity,
and the interval count is the number of changes since the previous readout of the status.
To access the statuses, use the following functions on the entity:

.. table:: CycloneDDS-CXX Status accessors

	+-----------------------+--------------------------------+----------------------------------+
	| Entity Type           | Status Entity                  | Accessor                         |
	+=======================+================================+==================================+
	| Topic                 | InconsistentTopicStatus        | inconsistent_topic_status        |
	+-----------------------+--------------------------------+----------------------------------+
	| DataWriter            | OfferedDeadlineMissedStatus    | offered_deadline_missed_status   |
	|                       +--------------------------------+----------------------------------+
	|                       | OfferedIncompatibleQosStatus   | offered_incompatible_qos_status  |
	|                       +--------------------------------+----------------------------------+
	|                       | LivelinessLostStatus           | liveliness_lost_status           |
	|                       +--------------------------------+----------------------------------+
	|                       | PublicationMatchedStatus       | publication_matched_status       |
	+-----------------------+--------------------------------+----------------------------------+
	| DataReader            | RequestedDeadlineMissedStatus  | requested_deadline_missed_status |
	|                       +--------------------------------+----------------------------------+
	|                       | RequestedIncompatibleQosStatus | requested_incompatible_status    |
	|                       +--------------------------------+----------------------------------+
	|                       | SampleRejectedStatus           | sample_rejected_status           |
	|                       +--------------------------------+----------------------------------+
	|                       | LivelinessChangedStatus        | liveliness_changed_status        |
	|                       +--------------------------------+----------------------------------+
	|                       | SubscriptionMatchedStatus      | subscription_matched_status      |
	|                       +--------------------------------+----------------------------------+
	|                       | SampleLostStatus               | sample_lost_status               |
	+-----------------------+--------------------------------+----------------------------------+

The following code fragment shows statuses that make a writer wait until readers are present:

.. code:: C++

	dds::pub::DataWriter<DataType> wr(publisher, topic);
	while (0 == wr.publication_matched_status().current_count())
		std::this_thread::sleep_for(std::chrono::milliseconds(20));

The writer polls the total number of readers that are receiving data from it at 20 millisecond intervals for as long as there are no readers.
