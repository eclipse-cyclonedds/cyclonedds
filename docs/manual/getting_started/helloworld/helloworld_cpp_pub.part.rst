HelloWorld publisher C++ source code
====================================

The ``Publisher.cpp`` contains the source that writes a *Hello World* message. 

The DDS publisher application code is almost symmetric to the subscriber, except 
that you must create a publisher and ``DataWriter``. To ensure data is 
written only when at least one matching reader is discovered, a synchronization
statement is added to the main thread. Synchronizing the main thread until a reader 
is discovered ensures we can start the publisher or subscriber program in any order.

.. code-block:: C++
    :linenos:

    #include <cstdlib>
    #include <iostream>
    #include <chrono>
    #include <thread>

    /* Include the C++ DDS API. */
    #include "dds/dds.hpp"

    /* Include data type and specific traits to be used with the C++ DDS API. */
    #include "HelloWorldData.hpp"

    using namespace org::eclipse::cyclonedds;

    int main() {
        try {
            std::cout << "=== [Publisher] Create writer." << std::endl;

            /* First, a domain participant is needed.
             * Create one on the default domain. */
            dds::domain::DomainParticipant participant(domain::default_id());

            /* To publish something, a topic is needed. */
            dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddsC++_helloworld_example");

            /* A writer also needs a publisher. */
            dds::pub::Publisher publisher(participant);

            /* Now, the writer can be created to publish a HelloWorld message. */
            dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

            /* For this example, we'd like to have a subscriber read
             * our message. This is not always necessary. Also, the way it is
             * done here is to illustrate the easiest way to do so. However, it is *not*
             * recommended to do a wait in a polling loop.
             * Please take a look at Listeners and WaitSets for much better
             * solutions, albeit somewhat more elaborate ones. */
            std::cout << "=== [Publisher] Waiting for subscriber." << std::endl;
            while (writer.publication_matched_status().current_count() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            /* Create a message to write. */
            HelloWorldData::Msg msg(1, "Hello World");

            /* Write the message. */
            std::cout << "=== [Publisher] Write sample." << std::endl;
            writer.write(msg);

            /* With a normal configuration (see dds::pub::qos::DataWriterQos
             * for various writer configurations), deleting a writer will
             * dispose of all its related messages.
             * Wait for the subscriber to have stopped to be sure it received the
             * message. Again, not normally necessary and not recommended to do
             * this in a polling loop. */
            std::cout << "=== [Publisher] Waiting for sample to be accepted." << std::endl;
            while (writer.publication_matched_status().current_count() > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        catch (const dds::core::Exception& e) {
            std::cerr << "=== [Publisher] Exception: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "=== [Publisher] Done." << std::endl;

        return EXIT_SUCCESS;
    }

To create a publisher:

#.  Send data using the ISOCPP DDS API and the ``HelloWorldData_Msg`` type, include the 
    appropriate header files:

    .. code-block:: C++

        #include "dds/dds.hpp"
        #include "HelloWorldData.hpp"

#. An exception handling mechanism ``try/catch`` block is used.

    .. code-block:: C++

        try {
            // …
        }
        catch (const dds::core::Exception& e) {
            std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

#.  Create a writer. You must have a participant, a topic, and a publisher (must have
    the same topic name as specified in ``subscriber.cpp``):

    .. code-block:: C++

        dds::domain::DomainParticipant participant(domain::default_id());
        dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddsC++_helloworld_example");
        dds::pub::Publisher publisher(participant);

#.  Create the writer for a specific topic ``“ddsC++_helloworld_example”`` in the 
    default DDS domain.

    .. code-block:: C++

        dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

#.  To modify the ``DataWriter`` Default Reliability Qos to Reliable:

    .. code-block:: C++

        dds::pub::qos::DataReaderQos dwqos = topic.qos() << dds::core::policy::Reliability::Reliable();
        dds::sub::DataWriter<HelloWorldData::Msg> dr(publisher, topic, dwqos);

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

        dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

#.  At regular intervals, the status change and a matching publication is received. In between, 
    the writing thread sleeps for for 20 milliseconds:

    .. code-block:: C++

        while (writer.publication_matched_status().current_count() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

    After this loop, a matching reader has been discovered.

#.  To write the data instance, create and initialize the data:

    .. code-block:: C++

        HelloWorldData::Msg msg(1, "Hello World");

#.  Send the data instance of the keyed message.

    .. code-block:: C++

        writer.write(msg);

#.  After writing the data to the writer, the *DDS C++ Hello World* example checks if 
    a matching subscriber(s) is still available. If matching subscribers exist, the 
    example waits for 50ms and starts publishing the data again. If no matching 
    subscriber is found, then the publisher program is ended:

    .. code-block:: C++

        return EXIT_SUCCESS;

    Through scoping, all the entities such as topic, writer, etc. are
    deleted automatically.
