..
   Copyright(c) 2006 to 2018 ADLINK Technology Limited and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. _`HelloWorldInDepth`:

.. raw:: latex

    \newpage


###########################
Hello World! in more detail
###########################

.. .. contents::

The previous chapter focused on building the *Hello World!* example while
this chapter will focus on the code itself; what has to be done to code
this small example.

.. _`HelloWorldDataType`:

*********************
Hello World! DataType
*********************

Data-Centric Architecture
=========================

By creating a Data-centric architecture, you get a loosely
coupled information-driven system. It emphasizes a data layer
that is common for all distributed applications within the
system. Because there is no direct coupling among the
applications in the DDS model, they can be added and removed
easily in a modular and scalable manner. This makes that the
complexity of a data-centric architecture doesn't really
increase when more and more publishers/subscribers are added.

The *Hello World!* example has a very simple 'data layer' of only
one data type :code:`HelloWorldData_Msg` (please read on).
The subscriber and publisher are not aware of each other.
The former just waits until somebody provides the data it
requires, while the latter just publishes the data without
considering the number of interested parties. In other words,
it doesn't matter for the publisher if there are none or
multiple subscribers (try running the *Hello World!* example
by starting multiple HelloworldSubscribers before starting a
HelloworldPublisher). A publisher just writes the data. The
DDS middleware takes care of delivering the data when needed.

******************
HelloWorldData.idl
******************

To be able to sent data from a writer to a reader, DDS needs to
know the data type. For the *Hello World!* example, this data type
is described using `IDL <http://www.omg.org/gettingstarted/omg_idl.htm>`_
and is located in HelloWorldData.idl. This IDL file will be compiled by
a IDL compiler which in turn generates a C language source and header
file. These generated source and header file will be used by the
HelloworldSubscriber and HelloworldPublisher in order to communicate
the *Hello World!* message between the HelloworldPublisher
and the HelloworldSubscriber.

Hello World! IDL
================

There are a few ways to describe the structures that make up the
data layer. The HelloWorld uses the IDL language to describe the
data type in HelloWorldData.idl:

.. literalinclude:: ../../../examples/helloworld/HelloWorldData.idl
    :linenos:
    :language: idl

An extensive explanation of IDL lies outside the scope of this
example. Nevertheless, a quick overview of this example is given
anyway.

First, there's the :code:`module HelloWorldData`. This is a kind
of namespace or scope or similar.
Within that module, there's the :code:`struct Msg`. This is the
actual data structure that is used for the communication. In
this case, it contains a :code:`userID` and :code:`message`.

The combination of this module and struct translates to the
following when using the c language.
::

    typedef struct HelloWorldData_Msg
    {
      int32_t userID;
      char * message;
    } HelloWorldData_Msg;

When it is translated to a different language, it will look
different and more tailored towards that language. This is the
advantage of using a data oriented language, like IDL, to
describe the data layer. It can be translated into different
languages after which the resulting applications can communicate
without concerns about the (possible different) programming
languages these application are written in.

.. _`IdlCompiler`:

Generate Sources and Headers
============================

Like already mentioned in the `Hello World! IDL`_ chapter, an IDL
file contains the description of data type(s). This needs to be
translated into programming languages to be useful in the
creation of DDS applications.

To be able to do that, there's a pre-compile step that actually
compiles the IDL file into the desired programming language.

A java application :code:`org.eclipse.cyclonedds.compilers.Idlc`
is supplied to support this pre-compile step. This is available
in :code:`idlc-jar-with-dependencies.jar`

The compilation from IDL into c source code is as simple as
starting that java application with an IDL file. In the case of
the *Hello World!* example, that IDL file is HelloWorldData.idl.
::

    java -classpath "<install_dir>/share/CycloneDDS/idlc/idlc-jar-with-dependencies.jar" org.eclipse.cyclonedds.compilers.Idlc HelloWorldData.idl

:Windows: The :code:`HelloWorldType` project within the HelloWorld solution.
:Linux: The :code:`make datatype` command.

This will result in new :code:`generated/HelloWorldData.c` and
:code:`generated/HelloWorldData.h`
files that can be used in the *Hello World!* publisher and
subscriber applications.

The application has to be rebuild when the data type source
files were re-generated.

Again, this is all for the native builds. When using CMake, all
this is done automatically.

.. _`HelloWorldDataFiles`:

HelloWorldData.c & HelloWorldData.h
===================================

As described in the :ref:`Hello World! DataType <HelloWorldDataType>`
paragraph, the IDL compiler will generate this source and header
file. These files contain the data type of the messages that are sent
and received.

While the c source has no interest for the application
developers, HelloWorldData.h contains some information that they
depend on. For example, it contains the actual message structure
that is used when writing or reading data.
::

    typedef struct HelloWorldData_Msg
    {
        int32_t userID;
        char * message;
    } HelloWorldData_Msg;

It also contains convenience macros to allocate and free memory
space for the specific data types.
::

    HelloWorldData_Msg__alloc()
    HelloWorldData_Msg_free(d,o)

It contains an extern variable that describes the data type to
the DDS middleware as well.
::

    HelloWorldData_Msg_desc

***************************
Hello World! Business Logic
***************************

Apart from the
:ref:`HelloWorldData data type files <HelloWorldDataFiles>` that
the *Hello World!* example uses to send messages, the *Hello World!*
example also contains two (user) source files
(:ref:`subscriber.c <HelloWorldSubscriberSource>` and
:ref:`publisher.c <HelloWorldPublisherSource>`), containing the
business logic.

.. _`HelloWorldSubscriberSource`:

*Hello World!* Subscriber Source Code
=====================================

Subscriber.c contains the source that will wait for a *Hello World!*
message and reads it when it receives one.

.. literalinclude:: ../../../examples/helloworld/subscriber.c
    :linenos:
    :language: c

We will be using the DDS API and the
:ref:`HelloWorldData_Msg <HelloWorldDataFiles>` type
to receive data. For that, we need to include the
appropriate header files.
::

    #include "dds/dds.h"
    #include "HelloWorldData.h"

The main starts with defining a few variables that will be used for
reading the *Hello World!* message.
The entities are needed to create a reader.
::

    dds_entity_t participant;
    dds_entity_t topic;
    dds_entity_t reader;

Then there are some buffers that are needed to actually read the
data.
::

    HelloWorldData_Msg *msg;
    void *samples[MAX_SAMPLES];
    dds_sample_info_t info[MAX_SAMPLES];

To be able to create a reader, we first need a participant. This
participant is part of a specific communication domain. In the
*Hello World!* example case, it is part of the default domain.
::

    participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);

The another requisite is the topic which basically describes the
data type that is used by the reader. When creating the topic,
the :ref:`data description <HelloWorldDataFiles>` for the DDS
middleware that is present in the
:ref:`HelloWorldData.h <HelloWorldDataFiles>` is used.
The topic also has a name. Topics with the same data type
description, but with different names, are considered
different topics. This means that readers/writers created with a
topic named "A" will not interfere with readers/writers created
with a topic named "B".
::

    topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
                              "HelloWorldData_Msg", NULL, NULL);

When we have a participant and a topic, we then can create
the reader. Since the order in which the *Hello World!* Publisher and
*Hello World!* Subscriber are started shouldn't matter, we need to create
a so called 'reliable' reader. Without going into details, the reader
will be created like this
::

    dds_qos_t *qos = dds_create_qos ();
    dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
    reader = dds_create_reader (participant, topic, qos, NULL);
    dds_delete_qos(qos);

We are almost able to read data. However, the read expects an
array of pointers to valid memory locations. This means the
samples array needs initialization. In this example, we have
an array of only one element: :code:`#define MAX_SAMPLES 1`.
So, we only need to initialize one element.
::

    samples[0] = HelloWorldData_Msg__alloc ();

Now everything is ready for reading data. But we don't know if
there is any data. To simplify things, we enter a polling loop
that will exit when data has been read.

Within the polling loop, we do the actual read. We provide the
initialized array of pointers (:code:`samples`), an array that
holds information about the read sample(s) (:code:`info`), the
size of the arrays and the maximum number of samples to read.
Every read sample in the samples array has related information
in the info array at the same index.
::

    ret = dds_read (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);

The :code:`dds_read` function returns the number of samples it
actually read. We can use that to determine if the function actually
read some data. When it has, then it is still possible that the
data part of the sample is not valid. This has some use cases
when there is no real data, but still the state of the related
sample has changed (for instance it was deleted). This will
normally not happen in the *Hello World!* example. But we check
for it anyway.
::

    if ((ret > 0) && (info[0].valid_data))

If data has been read, then we can cast the void pointer to the
actual message data type and display the contents. The polling
loop is quit as well in this case.
::

    msg = (HelloWorldData_Msg*) samples[0];
    printf ("=== [Subscriber] Received : ");
    printf ("Message (%d, %s)\n", msg->userID, msg->message);
    break;

When data is received and the polling loop is stopped, we need to
clean up.
::

    HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL);
    dds_delete (participant);

All the entities that are created using the participant are also
deleted. This means that deleting the participant will
automatically delete the topic and reader as well.


.. _`HelloWorldPublisherSource`:

*Hello World!* Publisher Source Code
====================================

Publisher.c contains the source that will write an *Hello World!* message
on which the subscriber is waiting.

.. literalinclude:: ../../../examples/helloworld/publisher.c
    :linenos:
    :language: c

We will be using the DDS API and the
:ref:`HelloWorldData_Msg <HelloWorldDataFiles>` type
to sent data. For that, we need to include the
appropriate header files.
::

    #include "dds/dds.h"
    #include "HelloWorldData.h"

Just like with the
:ref:`reader in subscriber.c <HelloWorldSubscriberSource>`,
we need a participant and a topic to be able to create a writer.
We use the same topic name as in subscriber.c. Otherwise the
reader and writer are not considered related and data will not
be sent between them.
::

    dds_entity_t participant;
    dds_entity_t topic;
    dds_entity_t writer;

    participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
    topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
                              "HelloWorldData_Msg", NULL, NULL);
    writer = dds_create_writer (participant, topic, NULL, NULL);

The DDS middleware is a publication/subscription implementation.
This means that it will discover related readers and writers
(i.e. readers and writers sharing the same data type and topic name)
and connect them so that written data can be received by readers
without the application having to worry about it. There is a catch though:
this discovery and coupling takes a small amount of
time. There are various ways to work around this problem. The following
can be done to properly connect readers and writers:

* Wait for the publication/subscription matched events

  * The Subscriber should wait for a subscription matched event
  * The Publisher should wait for a publication matched event.

  The use of these events will be outside the scope of this example

* Poll for the publication/subscription matches statuses

  * The Subscriber should poll for a subscription matched status to be set
  * The Publisher should poll for a publication matched status to be set

  The Publisher in this example uses the polling schema.

* Let the publisher sleep for a second before writing a sample. This
  is not recommended since a second may not be enough on several networks

* Accept that the reader miss a few samples at startup. This may be
  acceptable in cases where the publishing rate is high enough.

As said, the publisher of this example polls for the publication matched status.
To make this happen, the writer must be instructed to 'listen' for this status.
The following line of code makes sure the writer does so.
::

    dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);

Now the polling may start:
::

    while(true)
    {
        uint32_t status;
        ret = dds_get_status_changes (writer, &status);
        DDS_ERR_CHECK(ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        if (status == DDS_PUBLICATION_MATCHED_STATUS) {
            break;
        }
        /* Polling sleep. */
        dds_sleepfor (DDS_MSECS (20));
    }

After this loop, we are sure that a matching reader has been started.
Now, we commence to writing the data. First the data must be initialized
::

    HelloWorldData_Msg msg;

    msg.userID = 1;
    msg.message = "Hello World";

Then we can actually sent the message to be received by the
subscriber.
::

    ret = dds_write (writer, &msg);

After the sample is written, we need to clean up.
::

    ret = dds_delete (participant);

All the entities that are created using the participant are also
deleted. This means that deleting the participant will
automatically delete the topic and writer as well.
