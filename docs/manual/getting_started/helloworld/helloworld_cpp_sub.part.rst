HelloWorld subscriber C++ source code
=====================================

The ``Subscriber.cpp`` file mainly contains the statements to wait for a HelloWorld 
message and reads it when it receives it.

.. note::

    The |var-project-short| ``read`` semantics keep the data sample in the data 
    reader cache.

The subscriber application implements the steps defined in :ref:`key_steps`.

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

To create a subscriber:

#.  To recieve data using the DDS ISOCPP API and the ``HelloWorldData_Msg`` type, include
    the appropriate header files:

    - The ``dds.hpp`` file give access to the DDS APIs,
    - The ``HelloWorldData.hpp`` is specific to the data type defined
    in the IDL.

    .. code-block:: C++

        #include "dds/dds.hpp"
        #include "HelloWorldData.hpp"

#.  At least four DDS entities are needed to build a minimalistic application:

    - Domain participant
    - Topic
    - Subscriber
    - Reader

    .. code-block:: C++

        dds::domain::DomainParticipant participant(domain::default_id());
        dds::topic::Topic<HelloWorldData::Msg> topic(participant,"ddsC++_helloworld_example");
        dds::sub::Subscriber subscriber(participant);
        dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber,topic);

#.  The |var-project-short| C++ API simplifies and extends how data can be read or taken.
    To handle the data some, ``LoanedSamples`` is declared and created which loan samples 
    from the Service pool. Return of the loan is implicit and managed by scoping:

    .. code-block:: C++

        dds::sub::LoanedSamples<HelloWorldData::Msg> samples;
        dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;

#.  The ``read()/take()`` operation can return more than one data sample (where several 
    publishing applications are started simultaneously to write different message 
    instances), an an iterator is used:

        .. code-block:: C++

            const::HelloWorldData::Msg& msg;
            const dds::sub::SampleInfo& info;

#.  In |var-project-short|, data and metadata are propagated together. The samples are a 
    set of data samples (that is, user-defined data) and metadata describing the sample 
    state and validity, etc ,,, (``info``). To get the data and metadata from each sample, 
    use iterators:

    .. code-block:: C++

        try {
            // ...
        }
        catch (const dds::core::Exception& e) {
            std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

#.  To locate issues precisely when they occur, it is good practice to surround every 
    key verb of the DDS APIs with ``try/catch`` block. For example, the following shows
    how one block is used to facilitate the programming model of the applications and 
    improve source code readability:

    .. code-block:: C++

        dds::domain::DomainParticipant participant(domain::default_id());

#.  The DDS participant is always attached to a specific DDS domain. In the HelloWorld 
    example, it is part of the ``Default\_Domain``, which is specified in the XML deployment 
    file. To override the default behavior, create or edit a deployment file (for example, 
    ``$CYCLONEDDS_URI``).

#.  Create a subscriber attached to your participant.

    .. code-block:: C++

        dds::sub::Subscriber subscriber(participant);

#.  Create the topic with a given name (\ ``ddsC++_helloworld_example``) and the predefined 
    data type(\ ``HelloWorldData::Msg``). Topics with the same data type description 
    and with different names are considered other topics. This means that readers 
    or writers created for a given topic do not interfere with readers or writers 
    created with another topic even if they have the same data type.

    .. code-block:: C++

        dds::topic::Topic<HelloWorldData::Msg> topic(participant,"ddsC++_helloworld_example");

#.  Create a data reader and attach to it:

    .. code-block:: C++

        dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber, topic);

#.  To modify the data reader default reliability Qos to `reliable`:

    .. code-block:: C++

        dds::sub::qos::DataReaderQos drqos = topic.qos() << dds::core::policy::Reliability::Reliable();
        dds::sub::DataReader<HelloWorldData::Msg> dr(subscriber, topic, drqos);

#.  To retrieve data in your application code from the data reader's cache, you can 
    either:
    
    - Use a pre-allocated buffer to store the data. Create an array/vector-like like container. 
    - Loan it from the middleware. These buffers are actually 'owned' by the middleware,
      precisely by the ``DataReader``. The |var-project-short| C++ API implicitly allows
      you to return the loans through scoping.

    In the example, use the loan samples mode (``LoanedSamples``). ``Samples`` are 
    an unbounded sequence of samples; the sequence length depends on the amount of 
    data available in the data reader's cache:

    .. code-block:: C++

        dds::sub::LoanedSamples<HelloWorldData::Msg> samples;

#.  Attempt to read data by going into a polling loop that regularly scrutinizes 
    and examines the arrival of a message. Samples are removed from the reader's 
    cache when taken with the ``take()``:

    .. code-block:: C++

        samples = reader.take();

    If you choose to read the samples with ``read()``, data remains in the data 
    reader cache. A length() of samples greater than zero indicates that the data 
    reader cache was not empty:

    .. code-block:: C++

        if (samples.length() > 0)

#.  As sequences are **NOT** pre-allocated by the user, buffers are 'loaned' by the 
    ``DataReader``:

    .. code-block:: C++

        dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;
        for (sample_iter = samples.begin();
            sample_iter < samples.end();
            ++sample_iter)

#.  For each sample, cast and extract its user-defined data (``Msg``) and metadate 
    (``info``):

    .. code-block:: C++

        const HelloWorldData::Msg& msg = sample_iter->data();
        const dds::sub::SampleInfo& info = sample_iter->info();

    The SampleInfo (``info``) shows whether the data we are taking is
    *Valid* or *Invalid*. Valid data means that it contains the payload
    provided by the publishing application. Invalid data means that we are
    reading the DDS state of the data Instance. The state of a data instance can
    be ``DISPOSED`` by the writer, or it is ``NOT_ALIVE`` anymore, which
    could happen when the publisher application terminates while the
    subscriber is still active. In this case, the sample is not considered
    Valid, and its sample ``info.valid()`` field is False.

    .. code-block:: C++

        if (info.valid())

#.  Display the sample containing valid data:

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
data reader, subscriber, and topic are automatically deleted by
middleware through the scoping mechanism.
