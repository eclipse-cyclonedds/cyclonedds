## DDS-CXX _Hello World_ Subscriber Source Code

The Subscriber.cpp file mainly contains the statements to wait for a _Hello World_ message and reads it when it receives it.

Note that, the read sematic keeps the data sample in the Data Reader cache. Subscriber application basically implements the steps defined in [section 7.1.](Helloworld-CXX/keys-steps-to-build-the-hello-world-application.html)

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
        std::cout << "=== [Subscriber] Create reader." << std::endl;

        /* First, a domain participant is needed.
         * Create one on the default domain. */
        dds::domain::DomainParticipant participant(domain::default_id());

        /* To subscribe to something, a topic is needed. */
        dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddscxx_helloworld_example");

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
```


Within the subscriber code we will be mainly using the DDS ISOCPP API and the `HelloWorldData::Msg` type. Therefore, the following header files need to included:

- The **dds.hpp** file give access to the DDS APIs,
- The **HelloWorldData_DCPS.hpp** is specific to the data type defined in the IDL.

```
#include "dds/dds.hpp"
#include "HelloWorldData_DCPS.hpp"
```


At least four dds entities are needed, the domain participant, the topic , the subscriber and the reader.

```
dds::domain::DomainParticipant participant(domain::default_id());
dds::topic::Topic<HelloWorldData::Msg> topic(participant,"ddscxx_helloworld_example");
dds::sub::Subscriber subscriber(participant);
dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber,topic);
```


The Cyclone DDS-CXX API simplifies and extends the ways in which data can be read or taken. To handle the data some `LoanedSamples` are declared and created which loans samples from the Service pool. Return of the loan is implicit and managed by scoping:

```
dds::sub::LoanedSamples<HelloWorldData::Msg> samples;
dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;
```


As the `read( )/take()` operation may return more the one data sample (in the event that several publishing applications are started simultaneously to write different message instances), an iterator is rather used.

```
const::HelloWorldData::Msg& msg;
const dds::sub::SampleInfo& info;
```


In DDS data and metadata are propagated together. The samples are a set of the data-samples ( i.e user defined data) and metadata describing the sample state, validity etc ,,, (`info`). To get the data and its metadata from each of the samples, we can use iterators.

```
Try {
    …
}
catch (const dds::core::Exception& e) {
    std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
}
```


The good practice is to surround every key verbs of the DDS APIs with `try/catch` block to precisely locate issues when they occur. In this example one block is used to facilitate the programming model of the applications and improve their source code readability.

```
dds::domain::DomainParticipant participant(domain::default_id());
```


The dds participant is always attached to a specific dds domain. In the Hello World! example, it is part of the Default_Domain, the one specified in the xml deployment file that you have potentially be created ( i.e the one pointed `$CYCLONEDDS_URI`), please refers to section 1.6 for further details.

Subsequently, create a subscriber attached to your participant.

```
dds::sub::Subscriber subscriber(participant);
```


The next step is to create the topic with a given name(`ddscxx_helloworld_example`)and the predefined data type(`HelloWorldData::Msg`). Topics with the same data type description and with different names, are considered different topics. This means that readers or writers created for a given topic will not interfere with readers or writers created with another topic even though the same the same data type.

```
dds::topic::Topic<HelloWorldData::Msg> topic(participant,"ddscxx_helloworld_example");
```


Once the topic is created, we can create and associate to it a data reader.

```
dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber, topic);
```


To modify the Data Reader Default Reliability Qos to Reliable do:

```
dds::sub::qos::DataReaderQos drqos = topic.qos() << dds::core::policy::Reliability::Reliable();
dds::sub::DataReader<HelloWorldData::Msg> dr(subscriber, topic, drqos);
```


To retrieve data in your application code from the data reader's cache you can either use pre-allocated

a buffer to store the data or loaned it from the middleware.

If you decide to use a pre-allocated buffer to create array/vector like container. If you use the loaned buffer option, you need to be aware that these buffers are actually 'owned' by the middleware, precisely by the DataReader. The Cyclone DDS CXX API allows you also to return of the loans implicit through scoping.

In our example we are using the loan samples mode (`LoanedSamples`). `Samples` are an unbounded sequence of samples; the length of the sequence depends on the amount of data available in the data reader's cache.

```
dds::sub::LoanedSamples<HelloWorldData::Msg> samples;
```

At this stage we can attempt to read data by going into a polling loop that will regularly scrutinize and examine the arrival of a message. Samples are removed from the reader's cache when taken with the `take()`.

```
samples = reader.take();
```


If you choose to read the samples with `read()`, data remains in the data reader cache. A length() of samples greater the zero indicates that the data reader cache was not empty.

```
if (samples.length() > 0)
```


As sequences are NOT pre-allocated by the user, buffers will be 'loaned' to him by the DataReader.

```
dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;
for (sample_iter = samples.begin();
     sample_iter < samples.end();
     ++sample_iter)
```


For each individual sample, cast and extract it user defined data (`Msg`) and metadate (`info`).

```
const HelloWorldData::Msg& msg = sample_iter->data();
const dds::sub::SampleInfo& info = sample_iter->info();
```


The SampleInfo (`info`) will tell whether the data we are taking is _Valid_ or _Invalid_. Valid data means that it contains the payload provided by the publishing application. Invalid data means, that we are rather reading the DDS state of data Instance. The state of a data instance can be for instance `DISPOSED` by the writer or it is `NOT_ALIVE` anymore, which could happen for instance the publisher application terminates while the subscriber is still active. In this case the sample will not be considered as Valid, and its sample `info.valid()` field will be False.

```
if (info.valid())
```


As the sample contains valid data, we can safely display its content.

```
std::cout << "=== [Subscriber] Message received:" << std::endl;
std::cout << "    userID  : " << msg.userID() << std::endl;
std::cout << "    Message : \"" << msg.message() << "\"" << std::endl;
```


As we are using the Poll data reading mode, we repeat the above steps every 20 milliseconds.

```
else {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
}
```


This example uses the polling mode to read or take data. Cyclone DDS offers _waitSet_ and _Listener_ mechanism to notify the application that data is available in their cache, which avoid polling the cache at a regular interval. The discretion of these mechanisms is beyond the scope of this document.

All the entities that are created under the participant, such as the Data Reader Subscriber and Topic, are automatically deleted by middleware through the scoping mechanism.