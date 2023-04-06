.. index:: Publishers

.. _publishers_bm:

==========
Publishers
==========

A Publisher is a producer of data on a Domain. It uses the :ref:`domainparticipants_bm` to gain
access to the Domain and is created using it. That is, the Publisher passes down the 
Domain from its parent class DomainParticipant. A Publisher allows the :ref:`datawriters_bm` 
associated with it to share the same behaviour, for example:

- Liveliness notifications
- :ref:`qos_bm`
- :ref:`listeners_bm` callbacks

To use the default settings:

.. tabs::

    .. group-tab:: Core DDS (C)

      .. code-block:: C

         dds_entity_t publisher = dds_create_publisher (participant, NULL, NULL);

    .. group-tab:: C++

      .. code-block:: C++

			dds::pub::Publisher pub(participant);

    .. group-tab:: Python

      .. code-block:: python

         publisher = Publisher(participant)

To supply your own settings:

.. tabs::

    .. group-tab:: Core DDS (C)

      .. code-block:: C
			
         dds_qos_t *qos = dds_create_qos ();
         dds_listener_t *listener = dds_create_listener(NULL);
         dds_lset_publication_matched(listener, publication_matched);
         dds_entity_t publisher = dds_create_spublisher (participant, qos, listener);

    .. group-tab:: C++

      .. code-block:: C++

			dds::pub::NoOpPublisherListener listener; /*you need to create your own class that derives from this listener, and implement your own callbacks*/
			/*the listener implementation should implement the on_publication_matched virtual function as we will rely on it later*/
			dds::pub::qos::PublisherQos pubqos; /*add custom QoS policies that you want for this publisher*/
			dds::pub::Publisher pub(participant, pubqos, &listener, dds::core::status::StatusMask::publication_matched()); /*in this case, the only status we are interested in is publication_matched*/

    .. group-tab:: Python

      .. code-block:: python

			Python code sample TBD

.. note::
	Any :ref:`datawriters_bm` created using ``pub`` inherit the qos and listener functionality as set through it.
