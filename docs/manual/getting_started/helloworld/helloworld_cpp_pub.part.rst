
The ``Publisher.cpp`` contains the source that writes a *Hello World* message. 

The DDS publisher application code is almost symmetric to the subscriber, except 
that you must create a publisher and ``DataWriter``. To ensure data is 
written only when at least one matching reader is discovered, a synchronization
statement is added to the main thread. Synchronizing the main thread until a reader 
is discovered ensures we can start the publisher or subscriber program in any order.

The following is a copy of the **publisher.cpp** file that is available from the 
|url::helloworld_cpp_github| repository.

.. literalinclude:: publisher.cpp
    :linenos:

To create a publisher:

#.  Send data using the ISOCPP DDS API and the ``HelloWorldData_Msg`` type, include the 
    appropriate header files:

    .. code-block:: C++
        :linenos:
        :lineno-start: 18

        #include "dds/dds.hpp"

    .. code-block:: C++
        :linenos:
        :lineno-start: 21

        #include "HelloWorldData.hpp"

#.  An exception handling mechanism ``try/catch`` block is used.

    .. code-block:: C++
        :linenos:
        :lineno-start: 26

        try {

#.  Create a writer. You must have a participant, a topic, and a publisher (must have
    the same topic name as specified in ``subscriber.cpp``):

    .. code-block:: C++
        :linenos:
        :lineno-start: 31

        dds::domain::DomainParticipant participant(domain::default_id());

    .. code-block:: C++
        :linenos:
        :lineno-start: 34

        dds::topic::Topic<HelloWorldData::Msg> topic(participant, "HelloWorldData_Msg");

    .. code-block:: C++
        :linenos:
        :lineno-start: 37

        dds::pub::Publisher publisher(participant);

#.  Create the writer for a specific topic ``“ddsC++_helloworld_example”`` in the 
    default DDS domain.

    .. code-block:: C++
        :linenos:
        :lineno-start: 40

        dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

#.  When readers and writers are sharing the same data type and topic name, it connects 
    them without the application's involvement. To write data only when a DataReader 
    appears, a rendezvous pattern is required. A rendezvous pattern can be implemented by
    either:

    - Regularly polling the publication matching status (the preferred option in this 
      example).

    - Waiting for the publication/subscription matched events, where the publisher waits 
      and blocks the writing thread until the appropriate publication-matched event is 
      raised.
   
    The following line of code instructs |var-project-short| to listen on the
    ``writer.publication_matched_status()``:

    .. code-block:: C++
        :linenos:
        :lineno-start: 40

        dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

#.  At regular intervals, the status change and a matching publication is received. In between, 
    the writing thread sleeps for for 20 milliseconds:

    .. code-block:: C++
        :linenos:
        :lineno-start: 49

        while (writer.publication_matched_status().current_count() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

    After this loop, a matching reader has been discovered.

#.  To write the data instance, create and initialize the data:

    .. code-block:: C++
        :linenos:
        :lineno-start: 54

        HelloWorldData::Msg msg(1, "Hello World");

#.  Send the data instance of the keyed message.

    .. code-block:: C++
        :linenos:
        :lineno-start: 58

        writer.write(msg);

#.  After writing the data to the writer, the *DDS C++ Hello World* example checks if 
    a matching subscriber(s) is still available. If matching subscribers exist, the 
    example waits for 50ms and starts publishing the data again. If no matching 
    subscriber is found, then the publisher program is ended:

    .. code-block:: C++
        :linenos:
        :lineno-start: 78

        return EXIT_SUCCESS;

    Through scoping, all the entities such as topic, writer, etc. are
    deleted automatically.

.. note::
    To modify the ``DataWriter`` Default Reliability Qos to Reliable:

    .. code-block:: C++
        :linenos:
        :lineno-start: 60

        /* With a normal configuration (see dds::pub::qos::DataWriterQos

    .. code-block:: C++

        dds::pub::qos::DataReaderQos dwqos = topic.qos() << dds::core::policy::Reliability::Reliable();
        dds::sub::DataWriter<HelloWorldData::Msg> dr(publisher, topic, dwqos);