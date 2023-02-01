.. index:: DataReaders

.. _datareaders_bm:

===========
DataReaders
===========

DataReaders enable the user access to the data received by a :ref:`subscriber <subscribers_bm>` 
on a :ref:`topic <topic_bm>`, and takes as a template parameter the data type being 
exchanged. The settings for the reader are either inherited from the subscriber, or 
explicitly set in its own QoS policies and listener:

Inherited from the subscriber:

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code:: C++
			
			dds::sub::DataReader<DataType> reader(sub, topic);

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
			
			dds::sub::NoOpAnyDataReaderListener listener; /*you need to create your own class that derives from this listener, and implement your own callback functions*/
			/*the listener implementation should implement the on_data_available virtual function as we will rely on it later*/
			dds::sub::qos::DataReaderQos rqos;
			dds::sub::DataReader<DataType> reader(sub, topic, rqos, &listener, dds::core::status::StatusMask::data_available());

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

The data is accessed by either `reading` or `taking` the samples from the reader.
Both return a container of ``dds::sub::Sample``s, which have:

- The received sample of the exchanged datatype accessed through `data()`.
- The metadata for the received sample accessed through `info()`. 
 
The metadata contains such information as:

- Sample timestamp (time of writing).
- Data validity (whether the call to `data()` will return anything that should be processed).
- Sample state (READ/NOT_READ/...).

The difference between these two different access methods is the state of the reader 
after the access is finished. 

The `take` operation only returns samples that have not yet been returned in a `take` operation, 

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++
			:emphasize-lines: 1

			auto samples = reader.take();
			for (const auto & sample:samples) {
				if (!sample.valid())
					continue;
				const auto &data = sample.data();
				/*print the data?*/
			}

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

The `read` operation returns all samples currently stored by the reader.

.. tabs::

    .. group-tab:: Core DDS (C)

		.. code:: C
			
			C code sample TBD

    .. group-tab:: C++

		.. code-block:: C++
			:emphasize-lines: 1

			auto samples = reader.read();
			for (const auto & sample:samples) {
				if (!sample.valid() ||
					sample.state() != dds::sub::status::SampleState::not_read())
					continue;
				const auto &data = sample.data();
				/*print the data?*/
			}

    .. group-tab:: Python

		.. code:: Python

			Python code sample TBD

