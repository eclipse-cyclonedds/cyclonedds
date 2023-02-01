.. index:: Subscribers

.. _subscribers_bm:

===========
Subscribers
===========

A Subscriber is a consumer of data on a Domain. It uses the :ref:`domainparticipants_bm` 
to gain access to the Domain and is created using it. A Subscriber allows the 
:ref:`datareaders_bm` associated with it to share the same behaviour, such as:

- Liveliness notifications
- :ref:`qos_bm`
- :ref:`listeners_bm` callbacks

To use the default settings:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++

			dds::sub::Subscriber sub(participant);

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

To supply your own settings:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++

			dds::sub::NoOpSubscriberListener listener; /*you need to create your own class that derives from this listener, and implement your own callbacks*/
			/*the listener implementation should implement the on_subscription_matched virtual function as we will rely on it later*/
			dds::sub::qos::SubscriberQos subqos; /*add custom QoS policies that you want for this subscriber*/
			dds::sub::Subscriber sub(participant, subqos, &listener, dds::core::status::StatusMask::subscription_matched());

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

.. note::
	Any :ref:`datareaders_bm` created using ``sub`` inherit the qos and listener functionality as set through it.	
