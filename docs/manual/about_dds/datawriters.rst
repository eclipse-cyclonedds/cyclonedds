.. index:: DataWriters

.. _datawriters_bm:

===========
DataWriters
===========

DataWriters write data to a :ref:`topic <topic_bm>` using a 
:ref:`publisher <publishers_bm>`, and take as a template parameter the data type being 
exchanged. The settings for the writer are either inherited from the publisher, or 
explicitly set in its own :ref:`Qos <qos_bm>` policies and listener.

Inherited from the publisher:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++
			
			dds::pub::DataWriter<DataType> writer(pub, topic);

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

Explicitly set in its own QoS policies and listener:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++
			
			dds::pub::NoOpAnyDataWriterListener listener; /*you need to create your own class that derives from this listener, and implement your own callback functions*/
			/*the listener implementation should implement the on_publication_matched virtual function as we will rely on it later*/
			dds::pub::qos::DataWriterQos wqos;
			dds::pub::DataWriter<DataType> writer(pub, topic, wqos, &listener, dds::core::status::StatusMask::publication_matched());

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

A writer can write a sample:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++
			
			DataType sample;
			writer.write(sample);

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

A sample with a specific timestamp:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++
			
			DataType sample;
			dds::core::Time timestamp(123 /*seconds*/, 456 /*nanoseconds*/);
			writer.write(sample, timestamp);

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

A range of samples:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++
			
			std::vector<DataType> samples;
			writer.write(samples.begin(), samples.end());

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

.. tbd::
	Or update existing instances through handles, which we will not go into here.