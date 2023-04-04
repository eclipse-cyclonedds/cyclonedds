
The ``subscriber.cpp`` file mainly contains the statements to wait for a HelloWorld 
message and reads it when it receives it.

.. note::

    The |var-project-short| ``read`` semantics keep the data sample in the data 
    reader cache.

The subscriber application implements the steps defined in :ref:`key_steps`. 

The following is a copy of the **subscriber.cpp** file that is available from the 
|url::helloworld_cpp_github| repository.

.. literalinclude:: subscriber.cpp
    :linenos:

To create a subscriber:

#.  To recieve data using the DDS ISOCPP API and the ``HelloWorldData_Msg`` type, include
    the appropriate header files:

    - The ``dds.hpp`` file give access to the DDS APIs,
    - The ``HelloWorldData.hpp`` is specific to the data type defined in the IDL.

    .. code-block:: C++
        :linenos:
        :lineno-start: 18

        #include "dds/dds.hpp"

    .. code-block:: C++
        :linenos:
        :lineno-start: 21

        #include "HelloWorldData.hpp"

At least four DDS entities are needed to build a minimalistic application:

- Domain participant
- Topic
- Subscriber
- Reader

2.  The DDS participant is always attached to a specific DDS domain. In the HelloWorld 
    example, it is part of the ``Default\_Domain``, which is specified in the XML configuration 
    file. To override the default behavior, create or edit a configuration file (for example, 
    ``$CYCLONEDDS_URI``). For further information, refer to the :ref:`config-docs` 
    and the :ref:`configuration_reference`.

    .. code-block:: C++
        :linenos:
        :lineno-start: 31

        dds::domain::DomainParticipant participant(domain::default_id());
 
#.  Create the topic with a given name (\ ``ddsC++_helloworld_example``) and the predefined 
    data type (\ ``HelloWorldData::Msg``). Topics with the same data type description 
    and with different names are considered other topics. This means that readers 
    or writers created for a given topic do not interfere with readers or writers 
    created with another topic even if they have the same data type.

    .. code-block:: C++
        :linenos:
        :lineno-start: 34

        dds::topic::Topic<HelloWorldData::Msg> topic(participant, "HelloWorldData_Msg");

#.  Create the subscriber:
 
    .. code-block:: C++
        :linenos:
        :lineno-start: 37

        dds::sub::Subscriber subscriber(participant);

#.  Create a data reader and attach to it:

    .. code-block:: C++
        :linenos:
        :lineno-start: 40

        dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber, topic);

#.  The |var-project-short| C++ API simplifies and extends how data can be read or taken.
    To handle the data some, ``LoanedSamples`` is declared and created which loan samples 
    from the Service pool. Return of the loan is implicit and managed by scoping:

    .. code-block:: C++
        :linenos:
        :lineno-start: 53

        dds::sub::LoanedSamples<HelloWorldData::Msg> samples;

    .. code-block:: C++
        :linenos:
        :lineno-start: 61

        dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;

#.  The ``read()/take()`` operation can return more than one data sample (where several 
    publishing applications are started simultaneously to write different message 
    instances), an an iterator is used:

    .. code-block:: C++
        :linenos:
        :lineno-start: 66

            const HelloWorldData::Msg& msg = sample_iter->data();
            const dds::sub::SampleInfo& info = sample_iter->info();

#.  In |var-project-short|, data and metadata are propagated together. The samples are a 
    set of data samples (that is, user-defined data) and metadata describing the sample 
    state and validity, etc ,,, (``info``). To get the data and metadata from each sample, 
    use iterators:

    .. code-block:: C++
        :linenos:
        :lineno-start: 87

        } catch (const dds::core::Exception& e) {
            std::cerr << "=== [Subscriber] DDS exception: " << e.what() << std::endl;
            return EXIT_FAILURE;
        } catch (const std::exception& e) {
            std::cerr << "=== [Subscriber] C++ exception: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

#.  To locate issues precisely when they occur, it is good practice to surround every 
    key verb of the DDS APIs with ``try/catch`` block. For example, the following shows
    how one block is used to facilitate the programming model of the applications and 
    improve source code readability:

    .. code-block:: C++
        :linenos:
        :lineno-start: 26

        try {

    .. code-block:: C++
        :linenos:
        :lineno-start: 87

        } catch (const dds::core::Exception& e) {

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
        :linenos:
        :lineno-start: 53

        dds::sub::LoanedSamples<HelloWorldData::Msg> samples;

#.  Attempt to read data by going into a polling loop that regularly scrutinizes 
    and examines the arrival of a message. Samples are removed from the reader's 
    cache when taken with the ``take()``:

    .. code-block:: C++
        :linenos:
        :lineno-start: 56

        samples = reader.take();

    If you choose to read the samples with ``read()``, data remains in the data 
    reader cache. A length() of samples greater than zero indicates that the data 
    reader cache was not empty:

    .. code-block:: C++
        :linenos:
        :lineno-start: 59

        if (samples.length() > 0)

#.  As sequences are **NOT** pre-allocated by the user, buffers are 'loaned' by the 
    ``DataReader``:

    .. code-block:: C++
        :linenos:
        :lineno-start: 61

        dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;
        for (sample_iter = samples.begin();
            sample_iter < samples.end();
            ++sample_iter)

#.  For each sample, cast and extract its user-defined data (``Msg``) and metadate 
    (``info``):

    .. code-block:: C++
        :linenos:
        :lineno-start: 66

        const HelloWorldData::Msg& msg = sample_iter->data();
        const dds::sub::SampleInfo& info = sample_iter->info();

    The SampleInfo (``info``) shows whether the data we are taking is
    *Valid* or *Invalid*. Valid data means that it contains the payload
    provided by the publishing application. Invalid data means that we are
    reading the DDS state of the data Instance. The state of a data instance can
    be ``DISPOSED`` by the writer, or it is ``NOT_ALIVE`` anymore, which
    could happen when the publisher application terminates while the
    subscriber is still active. In this case, the sample is not considered
    valid, and its sample ``info.valid()`` field is False.

    .. code-block:: C++
        :linenos:
        :lineno-start: 74

        if (info.valid())

#.  Display the sample containing valid data:

    .. code-block:: C++
        :linenos:
        :lineno-start: 75

        std::cout << "=== [Subscriber] Message received:" << std::endl;
        std::cout << "    userID  : " << msg.userID() << std::endl;
        std::cout << "    Message : \"" << msg.message() << "\"" << std::endl;

    As we are using the Poll data reading mode, we repeat the above steps
    every 20 milliseconds.

    .. code-block:: C++
        :linenos:
        :lineno-start: 83

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

.. note::
    To modify the data reader default reliability Qos to `reliable`:

    .. code-block:: C++
        :linenos:
        :lineno-start: 60

        /* With a normal configuration (see dds::pub::qos::DataWriterQos

    .. code-block:: C++

        dds::sub::qos::DataReaderQos drqos = topic.qos() << dds::core::policy::Reliability::Reliable();
        dds::sub::DataReader<HelloWorldData::Msg> dr(subscriber, topic, drqos);
