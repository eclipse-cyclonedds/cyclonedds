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

		.. code-block:: C
			
			dds_entity_t reader = dds_create_reader (subscriber, topic, NULL, NULL);

    .. group-tab:: C++

		.. code-block:: C++
			
			dds::sub::DataReader<DataType> reader(sub, topic);

    .. group-tab:: Python

		.. code-block:: python

			reader = DataReader(subscriber, topic)

Explicitly set in its own QoS policies and listener:

.. tabs::

    .. group-tab:: Core DDS (C)

      .. code-block:: C

         dds_qos_t *qos = dds_create_qos ();
         dds_listener_t *listener = dds_create_listener(NULL);
         dds_lset_data_available(listener, data_available);
         dds_entity_t reader = dds_create_reader (participant, topic, qos, listener);

    .. group-tab:: C++

      .. code-block:: C++

         dds::sub::qos::DataReaderQos rqos;
         dds::sub::NoOpAnyDataReaderListener listener;
         dds::sub::DataReader<DataType> reader(sub, topic, rqos, &listener, dds::core::status::StatusMask::data_available());

    .. group-tab:: Python

      .. code-block:: python

         qos = Qos()
         listener = MyListener()
         reader = DataReader(participant, topic, qos=qos, listener=listener)

The data is accessed by either `reading` or `taking` the samples from the reader.
Both return a container of samples , which have:

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

      .. code-block:: C
         :emphasize-lines: 4

         int MAXSAMPLES 10;
         void *samples[MAX_SAMPLES];
         dds_sample_info_t infos[MAX_SAMPLES];
         int samples_received = dds_take (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
         for (int i = 0; i < samples_received; i++) {
           if (info[i].valid_data) {
              /*print the data*/
           } 
         }

    .. group-tab:: C++

		.. code-block:: C++
			:emphasize-lines: 1

			auto samples = reader.take();
			for (const auto & sample:samples) {
				if (!sample.valid())
					continue;
				const auto &data = sample.data();
				/*print the data*/
			}

    .. group-tab:: Python

      .. code-block:: python

         for sample in reader.take_iter(timeout=duration(milliseconds=10)):
            print(sample)

The `read` operation returns all samples currently stored by the reader.

.. tabs::

    .. group-tab:: Core DDS (C)

      .. code-block:: C
         :emphasize-lines: 4

         int MAXSAMPLES 10;
         void *samples[MAX_SAMPLES];
         dds_sample_info_t infos[MAX_SAMPLES];
         int samples_received = dds_read (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
         for (int i = 0; i < samples_received; i++) {
           if (info[i].valid_data) {
              /*print the data*/
           } 
         }

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

      .. code-block:: python

         for sample in reader.read_iter(timeout=duration(milliseconds=10)):
            print(sample)

