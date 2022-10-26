DDS C++ Hello World Code anatomy
================================

The previous chapter described the installation process that built
implicitly or explicitly the C++ *Hello World!* Example.

This chapter introduces the structural code of a simple system made by an
application that publishes keyed messages and another one that subscribes and
reads such data. Each message represents a data object that is uniquely identified
with a key and a payload.

.. _key_steps_helloworld_cpp:

Keys steps to build the **Hello World!** application in C++
------------------------------------------------------------

The *Hello World!* example has a very simple 'data layer' with a data
model made of one data type ``Msg`` which represents keyed messages (c,f
next subsection).

To exchange data, applications' business logic with |var-project-short| must:

1. Declare its subscription and involvement into a **DDS domain**. A
   DDS domain is an administrative boundary that defines, scopes, and
   gathers all the DDS applications, data, and infrastructure that needs
   to interconnect and share the same data space. Each DDS domain has a
   unique identifier. Applications declare their participation within a
   DDS domain by creating a **Domain Participant entity**.
2. Create a **Data topic** with the data type described in the data
   model. The data types define the structure of the Topic. The Topic is,
   therefore, an association between the topic name and datatype. QoSs
   can be optionally added to this association. Thus, a topic
   categorizes the data into logical classes and streams.
3. Create at least a **Publisher**, a **Subscriber**, and **Data
   Readers** and **Writers** object specific to the topic
   created earlier. Applications may want to change the default QoSs at
   this stage. In the Hello world! example, the ``ReliabilityQoS`` is
   changed from its default value (Best-effort) to Reliable.
4. Once the previous DDS computational objects are in place, the
   application logic can start writing or reading the data.

At the application level, readers and writers need not be aware of
each other. The reading application, now designated as application
Subscriber, polls the data reader periodically until a writing
application, designated as application Publisher,
provides the required data into the shared Topic, namely ``HelloWorldData_Msg``.

The data type is described using the OMG IDL.
Language <http://www.omg.org/gettingstarted/omg_idl.htm>`__ located in
``HelloWorldData.idl`` file. This IDL file is considered the Data Model
of our example.

This data model is preprocessed and compiled by |var-project-short| C++
IDL-Compiler to generate a C++ representation of the data as described
in Chapter 6. These generated source and header files are used by the
``HelloworldSubscriber.cpp`` and ``HelloworldPublisher.cpp``
application programs to share the *Hello World!* Message instance and
sample.

HelloWorld IDL
^^^^^^^^^^^^^^

As explained earlier, the benefits of using IDL language to define
data is to have a data model that is independent of the programming
languages. The ``HelloWorld.idl`` IDL file can
therefore be reused, it is compiled to be used within C++ DDS based
applications.

The HelloWorld data type is described in a language-independent way and
stored in the HelloWorldData.idl file.

.. code-block:: omg-idl

    module HelloWorldData
    {
        struct Msg
        {
            @key long userID;
            string message;
        };
    };

The data definition language used for DDS corresponds to a subset of
the OMG Interface Definition Language (IDL). In our simple example, the HelloWorld data
model is made of one module ``HelloWorldData``. A module can be seen
as a namespace where data with interrelated semantics are represented
together as a logical unit.

The struct Msg is the actual data structure that shapes the data used to
build the Topics. As already mentioned, a topic is an
association between a data type and a string name. The topic name is not
defined in the IDL file but is instead defined by the application business
logic at runtime.

In our case, the data type ``Msg`` contains two fields: ``userID`` and
``message`` payload. The ``userID`` is used to uniquely identify each message
instance. This is done using the ``@key`` annotation.

The |var-project-short| C++ IDL compiler translates module names into
namespaces and structure names into classes.

It also generates code for public accessor functions for all fields
mentioned in the IDL struct, separate public constructors, and a
destructor:

-  A default (empty) constructor that recursively invokes the
   constructors of all fields
-  A copy-constructor that performs a deep copy from the existing class
-  A move-constructor that moves all arguments to its members

The destructor recursively releases all fields. It also generates code
for assignment operators that recursively construct all fields based on
the parameter class (copy and move versions). The following code snippet is
provided without warranty: the internal format may change, but the API
delivered to your application code is stable.

.. code-block:: C++

    namespace HelloWorldData
    {
        class Msg OSPL_DDS_FINAL
        {
        public:
            int32_t userID_;
            std::string message_;

        public:
            Msg() :
                    userID_(0) {}

            explicit Msg(
                int32_t userID,
                const std::string& message) : 
                    userID_(userID),
                    message_(message) {}

            Msg(const Msg &_other) : 
                    userID_(_other.userID_),
                    message_(_other.message_) {}

    #ifdef OSPL_DDS_C++11
            Msg(Msg &&_other) : 
                    userID_(::std::move(_other.userID_)),
                    message_(::std::move(_other.message_)) {}
            Msg& operator=(Msg &&_other)
            {
                if (this != &_other) {
                    userID_ = ::std::move(_other.userID_);
                    message_ = ::std::move(_other.message_);
                }
                return *this;
            }
    #endif
            Msg& operator=(const Msg &_other)
            {
                if (this != &_other) {
                    userID_ = _other.userID_;
                    message_ = _other.message_;
                }
                return *this;
            }

            bool operator==(const Msg& _other) const
            {
                return userID_ == _other.userID_ &&
                    message_ == _other.message_;
            }

            bool operator!=(const Msg& _other) const
            {
                return !(*this == _other);
            }

            int32_t userID() const { return this->userID_; }
            int32_t& userID() { return this->userID_; }
            void userID(int32_t _val_) { this->userID_ = _val_; }
            const std::string& message() const { return this->message_; }
            std::string& message() { return this->message_; }
            void message(const std::string& _val_) { this->message_ = _val_; }
    #ifdef OSPL_DDS_C++11
            void message(std::string&& _val_) { this->message_ = _val_; }
    #endif
        };

    }

**Note:** When translated into a different programming language, the
data has a different representation specific to the target
language. For instance, as shown in chapter 3, in C, the Helloworld data
type is represented by a C structure. This highlights the advantage of using
neutral language like IDL to describe the data model. It can be
translated into different languages that can be shared between various
applications written in other programming languages.

The IDL compiler generated files
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The IDL compiler is a bison-based parser written in pure C and should be
fast and portable. It loads dynamic libraries to support different output
languages, but this is seldom relevant to you as a user. You can use
``CMake`` recipes as described above or invoke directly:

.. code-block:: bash

    idlc -l C++ HelloWorldData.idl

This results in the following new files that need to be compiled and
their associated object file linked with the Hello *World!* publisher
and subscriber application business logic:

-  ``HelloWorldData.hpp``
-  ``HelloWorldData.cpp``

When using CMake to build the application, this step is hidden and is
done automatically. For building with CMake, refer to `building the
HelloWorld example. <#build-the-dds-C++-hello-world-example>`__

``HelloWorldData.hpp`` and ``HelloWorldData.cpp`` files contain the data
type of messages that are shared.

DDS C++ Hello World Business Logic
----------------------------------

As well as from the ``HelloWorldData`` data type files that the *DDS C++
Hello World* example used to send messages, the *DDS C++ Hello World!*
example also contains two application-level source files
(``subscriber.cpp`` and ``publisher.cpp``), containing the business
logic.

DDS C++ **Hello World!** Subscriber Source Code
^^^^^^^^^^^^^^^^^^^^^^^^^^^=^^^^^^^^^^^^^^^^^^^^^

The ``Subscriber.cpp`` file mainly contains the statements to wait for a
*Hello World* message and reads it when it receives it.

.. note::

    The read sematic keeps the data sample in the Data Reader
    cache. The Subscriber application implements the steps defined in
    :ref:`Key Steps to build helloworld for C++ <key_steps_helloworld_cpp>`.

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
            std::cout << "=== [Subscriber] Create reader." << std::endl;

            /* First, a domain participant is needed.
             * Create one on the default domain. */
            dds::domain::DomainParticipant participant(domain::default_id());

            /* To subscribe to something, a topic is needed. */
            dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddsC++_helloworld_example");

            /* A reader also needs a subscriber. */
            dds::sub::Subscriber subscriber(participant);

            /* Now, the reader can be created to subscribe to a HelloWorld message. */
            dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber, topic);

            /* Poll until a message has been read.
             * It isn't really recommended to do this kind wait in a polling loop.
             * It's done here just to illustrate the easiest way to get data.
             * Please take a look at Listeners and WaitSets for much better
             * solutions, albeit somewhat more elaborate ones. */
            std::cout << "=== [Subscriber] Wait for message." << std::endl;
            bool poll = true;

            while (poll) {
                /* For this example, the reader will return a set of messages (aka
                 * Samples). There are other ways of getting samples from reader.
                 * See the various read() and take() functions that are present. */
                dds::sub::LoanedSamples<HelloWorldData::Msg> samples;

                /* Try taking samples from the reader. */
                samples = reader.take();

                /* Are samples read? */
                if (samples.length() > 0) {
                    /* Use an iterator to run over the set of samples. */
                    dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;
                    for (sample_iter = samples.begin();
                         sample_iter < samples.end();
                         ++sample_iter) {
                        /* Get the message and sample information. */
                        const HelloWorldData::Msg& msg = sample_iter->data();
                        const dds::sub::SampleInfo& info = sample_iter->info();

                        /* Sometimes a sample is read, only to indicate a data
                         * state change (which can be found in the info). If
                         * that's the case, only the key value of the sample
                         * is set. The other data parts are not.
                         * Check if this sample has valid data. */
                        if (info.valid()) {
                            std::cout << "=== [Subscriber] Message received:" << std::endl;
                            std::cout << "    userID  : " << msg.userID() << std::endl;
                            std::cout << "    Message : \"" << msg.message() << "\"" << std::endl;

                            /* Only 1 message is expected in this example. */
                            poll = false;
                        }
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            }
        }
        catch (const dds::core::Exception& e) {
            std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "=== [Subscriber] Done." << std::endl;

        return EXIT_SUCCESS;
    }

Within the subscriber code, we mainly use the DDS ISOCPP API and the
``HelloWorldData::Msg`` type. Therefore, the following header files must
be included:

-  The ``dds.hpp`` file give access to the DDS APIs,
-  The ``HelloWorldData.hpp`` is specific to the data type defined
   in the IDL.

.. code-block:: C++

    #include "dds/dds.hpp"
    #include "HelloWorldData.hpp"

At least four DDS entities are needed, the domain participant, 
the topic, the subscriber, and the reader.

.. code-block:: C++

    dds::domain::DomainParticipant participant(domain::default_id());
    dds::topic::Topic<HelloWorldData::Msg> topic(participant,"ddsC++_helloworld_example");
    dds::sub::Subscriber subscriber(participant);
    dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber,topic);

The |var-project-short| C++ API simplifies and extends how data can be read or
taken. To handle the data some, ``LoanedSamples`` is declared and
created which loan samples from the Service pool. Return of the loan is
implicit and managed by scoping:

.. code-block:: C++

    dds::sub::LoanedSamples<HelloWorldData::Msg> samples;
    dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;

As the ``read( )/take()`` operation may return more the one data sample
(if several publishing applications are started
simultaneously to write different message instances), an iterator is
used.

.. code-block:: C++

    const::HelloWorldData::Msg& msg;
    const dds::sub::SampleInfo& info;

In DDS, data and metadata are propagated together. The samples are a 
set of data samples (i.e., user-defined data) and metadata describing the
sample state and validity, etc ,,, (``info``). We can use iterators to 
get the data and metadata from each sample.


.. code-block:: C++

    try {
        // ...
    }
    catch (const dds::core::Exception& e) {
        std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

It is good practice to surround every key verb of the DDS APIs with
``try/catch`` block to locate issues precisely when they occur. In this
example, one block is used to facilitate the programming model of the
applications and improve their source code readability.

.. code-block:: C++

    dds::domain::DomainParticipant participant(domain::default_id());

The DDS participant is always attached to a specific DDS domain. In the
Hello World! example, it is part of the Default\_Domain, the one
specified in the XML deployment file that you potentially be created
(i.e., the one pointing to ``$CYCLONEDDS_URI``), please refer to
:ref:`testing your installation <test_your_installation>` for further details.

Subsequently, create a subscriber attached to your participant.

.. code-block:: C++

    dds::sub::Subscriber subscriber(participant);

The next step is to create the topic with a given
name(\ ``ddsC++_helloworld_example``)and the predefined data
type(\ ``HelloWorldData::Msg``). Topics with the same data type
description and with different names are considered different topics.
This means that readers or writers created for a given topic do not
interfere with readers or writers created with another topic, even if
they are the same data type.

.. code-block:: C++

    dds::topic::Topic<HelloWorldData::Msg> topic(participant,"ddsC++_helloworld_example");

Once the topic is created, we can create and associate to it a data
reader.

.. code-block:: C++

    dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber, topic);

To modify the Data Reader Default Reliability Qos to Reliable:

.. code-block:: C++

    dds::sub::qos::DataReaderQos drqos = topic.qos() << dds::core::policy::Reliability::Reliable();
    dds::sub::DataReader<HelloWorldData::Msg> dr(subscriber, topic, drqos);

To retrieve data in your application code from the data reader's cache
you can either use a pre-allocated buffer to store the data or loan it
from the middleware.

If you use a pre-allocated buffer, you create an array/vector-like 
like container. If you use the loaned buffer option, you need to be
aware that these buffers are actually 'owned' by the middleware,
precisely by the ``DataReader``. The |var-project-short| C++ API implicitly 
allows you to return the loans through scoping.


In our example, we use the loan samples mode (``LoanedSamples``).
``Samples`` are an unbounded sequence of samples; the sequence 
length depends on the amount of data available in the data reader's
cache.

.. code-block:: C++

    dds::sub::LoanedSamples<HelloWorldData::Msg> samples;

At this stage, we can attempt to read data by going into a polling loop
that regularly scrutinizes and examines the arrival of a message.
Samples are removed from the reader's cache when taken with the
``take()``.

.. code-block:: C++

    samples = reader.take();

If you choose to read the samples with ``read()``, data remains in the
data reader cache. A length() of samples greater than zero indicates
that the data reader cache was not empty.

.. code-block:: C++

    if (samples.length() > 0)

As sequences are NOT pre-allocated by the user, buffers are 'loaned' to
him by the ``DataReader``.

.. code-block:: C++

    dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;
    for (sample_iter = samples.begin();
         sample_iter < samples.end();
         ++sample_iter)

For each sample, cast and extract its user-defined data
(``Msg``) and metadate (``info``).

.. code-block:: C++

    const HelloWorldData::Msg& msg = sample_iter->data();
    const dds::sub::SampleInfo& info = sample_iter->info();

The SampleInfo (``info``) tells us whether the data we are taking is
*Valid* or *Invalid*. Valid data means that it contains the payload
provided by the publishing application. Invalid data means that we are
reading the DDS state of the data Instance. The state of a data instance can
be ``DISPOSED`` by the writer, or it is ``NOT_ALIVE`` anymore, which
could happen when the publisher application terminates while the
subscriber is still active. In this case, the sample is not considered
Valid, and its sample ``info.valid()`` field is False.

.. code-block:: C++

    if (info.valid())

As the sample contains valid data, we can safely display its content.

.. code-block:: C++

    std::cout << "=== [Subscriber] Message received:" << std::endl;
    std::cout << "    userID  : " << msg.userID() << std::endl;
    std::cout << "    Message : \"" << msg.message() << "\"" << std::endl;

As we are using the Poll data reading mode, we repeat the above steps
every 20 milliseconds.

.. code-block:: C++

    else {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

This example uses the polling mode to read or take data. |var-project-short|
offers *waitSet* and *Listener* mechanism to notify the application that
data is available in their cache, which avoids polling the cache at a
regular intervals. The discretion of these mechanisms is beyond the
scope of this document.

All the entities that are created under the participant, such as the
Data Reader Subscriber and Topic are automatically deleted by
middleware through the scoping mechanism.

DDS C++ **Hello World!** Publisher Source Code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``Publisher.cpp`` contains the source that writes a *Hello World*
message. From the DDS perspective, the publisher application code is
almost symmetrical to the subscriber one, except that you need to create
a Publisher and ``DataWriter``, respectively, instead of a Subscriber and
Data Reader. A synchronization statement is added to the main thread to 
ensure data is only written when |var-project-short| discovers at least a matching 
reader. Synchronizing the main thread until a reader is discovered
assures we can start the publisher or subscriber program in any order.

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

We are using the ISOCPP DDS API and the HelloWorldData to receive data.
For that, we need to include the appropriate header files.

.. code-block:: C++

    #include "dds/dds.hpp"
    #include "HelloWorldData.hpp"

An exception handling mechanism ``try/catch`` block is used.

.. code-block:: C++

    try {
        // …
    }
    catch (const dds::core::Exception& e) {
        std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

As with the reader in ``subscriber.cpp``, we need a participant, a
topic, and a publisher to create a writer. We must also 

use the same topic name specified in the ``subscriber.cpp``.

.. code-block:: C++

    dds::domain::DomainParticipant participant(domain::default_id());
    dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddsC++_helloworld_example");
    dds::pub::Publisher publisher(participant);

With these entities ready, the writer can now be created. The writer is
created for a specific topic ``“ddsC++_helloworld_example”`` in the
default DDS domain.

.. code-block:: C++

    dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

To modify the ``DataWriter`` Default Reliability Qos to Reliable:

.. code-block:: C++

    dds::pub::qos::DataReaderQos dwqos = topic.qos() << dds::core::policy::Reliability::Reliable();
    dds::sub::DataWriter<HelloWorldData::Msg> dr(publisher, topic, dwqos);

When |var-project-short| discovers readers and writers sharing the same data
type and topic name, it connects them without the application's
involvement. A rendezvous pattern is required to write data only when 
a data reader appears. Either can implement such a pattern:

1. Wait for the publication/subscription matched events, where the
   Publisher waits and blocks the writing thread until the appropriate
   publication-matched event is raised, or
2. Regularly poll the publication matching status. This is the
   preferred option used in this example. The following line of code
   instructs |var-project-short| to listen on the
   ``writer.publication_matched_status()``

.. code-block:: C++

    dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

At regular intervals, we get the status change and for a matching
publication. In between, the writing thread sleeps for 20 milliseconds.

.. code-block:: C++

    while (writer.publication_matched_status().current_count() == 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

After this loop, we are confident that a matching reader has been
discovered. Now, we can commence the writing of the data instance.
First, the data must be created and initialized.

.. code-block:: C++

    HelloWorldData::Msg msg(1, "Hello World");

Send the data instance of the keyed message.

.. code-block:: C++

    writer.write(msg);

After writing the data to the writer, the *DDS C++ Hello World* example
checks if a matching subscriber(s) is still available. If 
matching subscribers exist, the example waits for 50ms and starts
publishing the data again. If no matching subscriber is found, then the
publisher program is ended.

.. code-block:: C++

    return EXIT_SUCCESS;

Through scoping, all the entities such as topic, writer, etc. are
deleted automatically.
