.. index:: ! Topics

.. _topics_bm:

======
Topics
======

A topic is a subsection of a DDS Domain that enables exchange of data of a specific 
type, and that complies with certain restrictions on the exchange before exchange can 
occur. 

A topic is identifiable by:

- **Name**: Identifies the topic on the Domain. Must be unique on the Domain.
- **Quality of Service** (optional): Determines the restrictions on the exchange. This is 
  an optional parameter, which can be derived from fallbacks to the participant or 
  defaults.

- **Type**: The type of data being exchanged, which is the template parameter of the 
  topic class:

.. tabs::

    .. group-tab:: Core DDS (C)

		:ref:`topic_bm`

    .. group-tab:: C++

		.. code-block:: C++

				template <typename T> class Topic {
				...
				// Constructor
					Topic(const dds::domain::DomainParticipant& dp,
						const std::string& name,
						const std::string& type_name,
						const dds::topic::qos::TopicQos& qos,
						dds::topic::TopicListener<T>* listener,
						const dds::core::status::StatusMask& mask);
				...
				}

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD



A topic is for exchanging data of the type **Data_Type**. The following shows how a 
topic is created on the DomainParticipant participant:


.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			dds_entity_t topic = dds_create_topic (participant, &DataType_desc, "DataType", NULL, NULL);

    .. group-tab:: C++

		.. code:: C++
			
			dds::topic::Topic<Data_Type> topic(participant, "DataType Topic");

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

To generate the data type of the topic from the user's IDL files, use |var-project-short| ``idlc`` 
generator (with the idlcxx library). 

.. important::
	Using types other than those generated from ``idlc`` + ``idlcxx`` in the template 
	does not have the prerequisite traits, and therefore does not result in working code.
