## DDS-CXX _Hello World_ Publisher Source Code

The **Publisher.cpp** contains the source that will write a _Hello World_ message. From the DDS perspective, the publisher application code is almost symmetric to the subscriber one, except that you need to create respectively a Publisher and Data Writer instead of a Subscriber and Data Reader. To assure data is written only when Cyclone DDS discovers at least a matching reader, a synchronization statement is added to the main thread. Synchronizing the main thread till a reader is discovered assures we can start the publisher or subscriber program in any order.

```
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>

/* Include the C++ DDS API. */
#include "dds/dds.hpp"

/* Include data type and specific traits to be used with the C++ DDS API. */
#include "HelloWorldData_DCPS.hpp"

using namespace org::eclipse::cyclonedds;

int main() {
    try {
        std::cout << "=== [Publisher] Create writer." << std::endl;

        /* First, a domain participant is needed.
         * Create one on the default domain. */
        dds::domain::DomainParticipant participant(domain::default_id());

        /* To publish something, a topic is needed. */
        dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddscxx_helloworld_example");

        /* A writer also needs a publisher. */
        dds::pub::Publisher publisher(participant);

        /* Now, the writer can be created to publish a HelloWorld message. */
        dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

        /* For this example, we'd like to have a subscriber to actually read
         * our message. This is not always necessary. Also, the way it is
         * done here is just to illustrate the easiest way to do so. It isn't
         * really recommended to do a wait in a polling loop, however.
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
         * for various different writer configurations), deleting a writer will
         * dispose all its related message.
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
```


We will be using the ISOCPP DDS API and the HelloWorldData to receive data. For that, we need to include the appropriate header files.

```
#include "dds/dds.hpp"
#include "HelloWorldData_DCPS.hpp"
```


An exception handling mechanism is used `try/catch` block is used. 

```
Try {
    …
}
catch (const dds::core::Exception& e) {
    std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
}
```

Like with the reader in **subscriber.cpp**, we need a participant, a topic, and a publisher to be able to create a writer. We also need to use the same topic name as the one specified in the subscriber.cpp.

```
dds::domain::DomainParticipant participant(domain::default_id());
dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddscxx_helloworld_example");
dds::pub::Publisher publisher(participant);
```


With these entities ready, the writer can now be created. The writer is created for a specific topic `“ddscxx_helloworld_example”` in the default DDS domain.

```
dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);
```


To modify the Data Writer Default Reliability Qos to Reliable do:

```
dds::pub::qos::DataReaderQos dwqos = topic.qos() << dds::core::policy::Reliability::Reliable();

dds::sub::DataWriter<HelloWorldData::Msg> dr(publisher, topic, dwqos);
```


When Cyclone DDS discovers readers and writers sharing the same data type and topic name, it connects them without the application involvement. In order to write data only when a Data Readers pops up, a sort of Rendez-vous pattern is required. Such pattern can be implemented using:

1. Wait for the publication/subscription matched events, where the Publisher waits and blocks the writing-thread until the appropriate publication matched event is raised, or
2. Regularly, polls the publication matching status. This is the preferred option we will implement in this example.The following line of code instructs Cyclone DDS to listen on the `writer.publication_matched_status()`.

```
dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);
```


At regular intervals, we get the status change and for a matching publication. In between, the writing thread sleeps for 20 milliseconds.

```
while (writer.publication_matched_status().current_count() == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
}
```


After this loop, we are sure that a matching reader has been discovered. Now, we commence the writing of the data instance. First, the data must be created and initialized

```
HelloWorldData::Msg msg(1, "Hello World");
```


Then we can send the data instance of the keyed message.

```
writer.write(msg);
```


After writing the data to the writer, the _DDS-CXX Hello World_ example check if there still have matching subscriber(s) available. If there is a matching subscriber(s), the example waits for 50ms and start publishing the data again. If no matching subscriber is found, then the publisher program is ended.

```
return EXIT_SUCCESS;
```


Through scoping, all the entities such as topic, writer … etc are deleted automatically.