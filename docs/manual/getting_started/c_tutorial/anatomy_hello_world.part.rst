**Hello World!** Code anatomy
=============================

The previous chapter described the installation process that built
implicitly or explicitly the C **Hello World!** Example.

This chapter introduces the fundamental concept of DDS. It details the structural code of a
simple system made by an application that publishes keyed messages and
another one that subscribes and reads such data. Each message represents
a data object that is uniquely identified with a unique key and a
payload.

Data-Centric Architecture
-------------------------

In a service-centric architecture, applications need to
know each other's interfaces to share data, share events, and share
commands or replies to interact. These interfaces are modeled as sets of 
operations and functions that are managed in centralized repositories. 
This type of architecture creates unnecessary dependencies that form a
tightly coupled system. The centralized interface repositories are
usually seen as a single point of failure.

In a data-centric architecture, your design focuses on the data each
the application produces and decides to share rather than on the Interfaces'
operations and the internal processing that produced them.

A data-centric architecture creates a decoupled system that focuses on
the data and applications states' that need to be shared rather than the
applications' details. In a data-centric system, data and their
associated quality of services are the only contracts that bound the
applications together. With DDS, the system decoupling is
bi-dimensional, in both space and time.

Space-decoupling derives from the fact that applications do not need to 
know the identity of the data produced or consumed, nor their logical 
or a physical location in the network. Under the hood, DDS
runs a zero-configuration, interoperable discovery protocol that
searches matching data readers and data writes that are interested in
the same data topic.

Time-decoupling derives from the fact that, fundamentally, the nature of
communication is asynchronous. Data producers and consumers,
known as ``DataWriter``s and ``DataReader``s, are not forced to
be active and connected simultaneously to share data. In this
scenario, the DDS middleware can handle and manage data on behalf of
late joining ``DataReader`` applications and deliver it to them when they
join the system.

Time and space decoupling gives applications the freedom to be plugged
or unplugged from the system at any time, from anywhere, in any order.
This keeps the complexity and administration of a data-centric
architecture relatively low when adding more and more ``DataReader`` and
``DataWriter`` applications.

.. _key_steps:

Keys steps to build the **Hello World!** application
----------------------------------------------------

The **Hello World!** example has a minimal 'data layer' with a data
model made of one data type ``Msg`` that represents keyed messages (c,f
next subsection).

To exchange data with |var-project|, applications' business logic needs
to:

1. Declare its participation and involvement in a *DDS domain*. A DDS
   domain is an administrative boundary that defines, scopes, and
   gathers all the DDS applications, data, and infrastructure that must 
   interconnect by sharing the same data space. Each DDS
   domain has a unique identifier. Applications declare their
   participation within a DDS domain by creating a **Domain Participant
   entity**.
2. Create a **Data topic** with the data type described in a data
   model. The data types define the structure of the Topic. The Topic is
   therefore, an association between the topic's name and a datatype.
   QoSs can be optionally added to this association. The concept Topic
   therefore discriminates and categorizes the data in logical classes
   and streams.
3. Create the **Data Readers** and **Writers** entities 
   specific to the topic. Applications may want to change the default
   QoSs. In the ``Hello world!`` Example, the ``ReliabilityQoS`` is changed
   from its default value (``Best-effort``) to ``Reliable``.
4. Once the previous DDS computational entities are in place, the
   application logic can start writing or reading the data.

At the application level, readers and writers do not need to be aware of
each other. The reading application, now called Subscriber, polls the
data reader periodically until a publishing application, now called
The publisher writes the required data into the shared topic, namely
``HelloWorldData_Msg``.

The data type is described using the OMG IDL.
Language <http://www.omg.org/gettingstarted/omg_idl.htm>`__ located in
``HelloWorldData.idl`` file. Such IDL file is seen as the data model of
our example.

This data model is preprocessed and compiled by the IDL Compiler
to generate a C representation of the data as described in Chapter 2.
These generated source and header files are used by the
``HelloworldSubscriber.c`` and ``HelloworldPublishe.c`` programs to
share the **Hello World!** Message instance and sample.

Hello World! IDL
^^^^^^^^^^^^^^^^

The HelloWorld data type is described in a language-independent way and
stored in the HelloWorldData.idl file:

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
model is made of one module ``HelloWorldData``. A module can be seen as
a namespace where data with interrelated semantics are represented
together in the same logical set.

The ``structMsg`` is the data type that shapes the data used to
build topics. As already mentioned, a topic is an association between a
data type and a string name. The topic name is not defined in the IDL
file, but the application business logic determines it at runtime.

In our simplistic case, the data type Msg contains two fields:
``userID`` and ``message`` payload. The ``userID`` is used to uniquely identify each message instance. This is done using the
``@key`` annotation.

The IDL compiler translates the IDL datatype into a C struct
with a name made of the\ ``<ModuleName>_<DataTypeName>`` .

.. code-block:: C

    typedef struct HelloWorldData_Msg
    {
        int32_t userID;
        char * message;
    } HelloWorldData_Msg;

.. note::

    When translated into a different programming language, the
    data has another representation specific to the target
    language. For instance, as shown in chapter 7, in C++, the Helloworld
    data type is represented by a C++ class. This highlights the advantage of using
    a neutral language like IDL to describe the data model. It can be
    translated into different languages that can be shared between different
    applications written in other programming languages.

Generated files with the IDL compiler
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The IDL compiler is a C program that processes .idl files.

.. code-block:: bash

    idlc HelloWorldData.idl


This results in new ``HelloWorldData.c`` and ``HelloWorldData.h`` files
that need to be compiled, and their associated object file must be linked
with the **Hello World!** publisher and subscriber application business
logic. When using the provided CMake project, this step is done automatically.

As described earlier, the IDL compiler generates one source and one
header file. The header file (``HelloWorldData.h``) contains the shared messages' data type. While the source file has no
direct use from the application developer's perspective.

``HelloWorldData.h``\ \* needs to be included in the application code as
it contains the actual message type and contains helper macros to
allocate and free memory space for the ``HelloWorldData_Msg`` type.

.. code-block:: C

    typedef struct HelloWorldData_Msg
    {
        int32_t userID;
        char * message;
    } HelloWorldData_Msg;

    HelloWorldData_Msg_alloc()
    HelloWorldData_Msg_free(d,o)

The header file also contains an extra variable that describes the data
type to the DDS middleware. This variable needs to be used by the
application when creating the topic.


.. code-block:: C

    HelloWorldData_Msg_desc


The Hello World! Business Logic
-------------------------------


As well as the ``HelloWorldData.h/c`` generated files, the *Hello
World!* example also contains two application-level source files
(``subscriber.c`` and ``publisher.c``), containing the business logic.


**Hello World!** Subscriber Source Code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``Subscriber.c`` mainly contains the statements to wait for a *Hello
World!* message and reads it when it receives it.

.. note::

    The |var-project-short| ``read`` semantics keep the data sample
    in the data reader cache. It is important to remember to use ``take`` where
    appropriate to prevent resource exhaustion.

The subscriber application implements the steps defined in :ref:`the Key Steps <key_steps>`.


.. code-block:: C
    :linenos:

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>

    /* An array of one message (aka sample in dds terms) will be used. */
    #define MAX_SAMPLES 1
    int main (int argc, char ** argv) {
      dds_entity_t participant;
      dds_entity_t topic;
      dds_entity_t reader;
      HelloWorldData_Msg *msg;
      void *samples[MAX_SAMPLES];
      dds_sample_info_t infos[MAX_SAMPLES];
      dds_return_t ret;
      dds_qos_t *qos;
      (void)argc;
      (void)argv;

      /* Create a Participant. */
      participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
      DDS_ERR_CHECK (participant, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      /* Create a Topic. */
      topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
      "HelloWorldData_Msg", NULL, NULL);
      DDS_ERR_CHECK (topic, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      /* Create a reliable Reader. */
      qos = dds_create_qos ();
      dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
      reader = dds_create_reader (participant, topic, qos, NULL);
      DDS_ERR_CHECK (reader, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
      dds_delete_qos(qos);

      printf ("\n=== [Subscriber] Waiting for a sample ...\n");

      /* Initialize the sample buffer, by pointing the void pointer within
      * the buffer array to a valid sample memory location. */
      samples[0] = HelloWorldData_Msg alloc ();

      /* Poll until data has been read. */
      while (true)
      {
        /* Do the actual read.
        * The return value contains the number of samples read. */
        ret = dds_read (reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Check if we read some data and it is valid. */
        if ((ret > 0) && (infos[0].valid_data))
        {
            /* Print Message. */
            msg = (HelloWorldData_Msg*) samples[0];
            printf ("=== [Subscriber] Received : ");
            printf ("Message (%d, %s)\n", msg->userID, msg->message);
            break;
        }
        else
        {
            /* Polling sleep. */
            dds_sleepfor (DDS_MSECS (20));
        }
      }
      /* Free the data location. */
      HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL);
      /* Deleting the participant will delete all its children recursively as well. */
      ret = dds_delete (participant);
      DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      return EXIT_SUCCESS;
    }

Within the subscriber code, we mainly use the DDS API and the
``HelloWorldData_Msg`` type. The following header files must be included:

* The ``dds.h`` file to give access to the DDS APIs
* The ``HelloWorldData.h`` is specific to the data type defined in the IDL

.. code-block:: C

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"

With |var-project-short|, at least three DDS entities are needed to build a
minimalistic application, the domain participant, the topic, and the
reader. |var-project-short| implicitly creates a DDS Subscriber. If
required, this behavior can be overridden.

.. code-block:: C

    dds_entity_t participant;
    dds_entity_t topic;
    dds_entity_t reader;

To handle the data, some buffers are declared and created:

.. code-block:: C

    HelloWorldData_Msg *msg;
    void *samples[MAX_SAMPLES];
    dds_sample_info_t info[MAX_SAMPLES];

As the ``read()`` operation may return more than one data sample (in the
event that several publishing applications are started simultaneously to
write different message instances), an array of samples is therefore
needed.

In |var-project-short| data and metadata are propagated together. The
``dds_sample_info`` array needs to be declared to handle the metadata.

The DDS participant is always attached to a specific DDS domain. In the
**Hello World!** example, it is part of the ``Default_Domain``, the one
specified in the XML deployment file (see :ref:`test your installation <test_your_installation>` for more details).

.. code-block:: C

    participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);

The next step is to create the topic with a given name. Topics with the
same data type description and with different names are considered
other topics. This means that readers or writers created for a given
topic do not interfere with readers or writers created with another
topic even if they have the same data type. Topics with the same name but
incompatible datatype are considered an error and should be avoided.

.. code-block:: C

    topic = dds_create_topic (participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);

Once the topic is created, we can create a data reader and attach to it.

.. code-block:: C

    dds_qos_t *qos = dds_create_qos ();
    dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
    reader = dds_create_reader (participant, topic, qos, NULL);
    dds_delete_qos(qos);

The read operation expects an array of pointers to a valid memory
location. This means the samples array needs initialization by pointing
the void pointer within the buffer array to a valid sample memory
location.

In our example, we have an array of one element;
(``#define MAX_SAMPLES 1``.) we only need to allocate memory for one
``HelloWorldData_Msg``.

.. code-block:: C

    samples[0] = HelloWorldData_Msg_alloc ();

At this stage, we can attempt to read data by going into a polling loop
that regularly scrutinizes and examines the arrival of a message.

.. code-block:: C

    ret = dds_read (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);

The ``dds_read`` operation returns the number of samples equal to the
parameter ``MAX_SAMPLE``. If data has arrived in the reader's cache, 
that number will exceed 0.

The Sample\_info (``info``) structure tells us whether the data we read 
is *Valid* or *Invalid*. Valid data means that it contains the
payload provided by the publishing application. Invalid data means we 
are reading the DDS state of the data Instance. The state of a
data instance can be, for instance, *DISPOSED* by the writer, or it is
*NOT\_ALIVE* anymore, which could happen if the publisher application
terminates while the subscriber is still active. In this case, the
sample is not considered Valid, and its sample ``info[].Valid_data``
the field is ``False``.


.. code-block:: C

    if ((ret > 0) && (info[0].valid_data))

If data is read, then we can cast the void pointer to the actual message
data type and display the contents.


.. code-block:: C

    msg = (HelloWorldData_Msg*) samples[0]; 
    printf ("=== [Subscriber] Received : ");
    printf ("Message (%d, %s)\n", msg->userID, msg->message);
    break;

When data is received and the polling loop is stopped, we release the
allocated memory and delete the domain participant.

.. code-block:: C

    HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL); 
    dds_delete (participant);

All the entities that are created under the participant, such as the
data reader and topic, are recursively deleted.

**Hello World!** Publisher Source Code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``Publisher.c`` contains the source that writes a **Hello World!**
Message.

From the DDS perspective, the publisher application code is almost
symmetric to the subscriber one, except that you need to create a data
writer instead of a data reader. To ensure data is written only when at
least one matching reader is discovered, a synchronization
statement is added to the main thread. Synchronizing the main thread
until a reader is discovered ensures we can start the publisher or
subscriber program in any order.


.. code-block:: C
    :linenos:

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"
    #include <stdio.h>
    #include <stdlib.h>

    int main (int argc, char ** argv)
    {
        dds_entity_t participant; 
        dds_entity_t topic; 
        dds_entity_t writer; 
        dds_return_t ret;
        HelloWorldData_Msg msg; 
        (void)argc;
        (void)argv;

        /* Create a Participant. */
        participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL); 
        DDS_ERR_CHECK (participant, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Create a Topic. */
        topic = dds_create_topic (participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL); 
        DDS_ERR_CHECK (topic, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Create a Writer. */
        writer = dds_create_writer (participant, topic, NULL, NULL);

        printf("=== [Publisher] Waiting for a reader to be discovered ...\n");

        ret = dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS); 
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        while(true)
        {
            uint32_t status;
            ret = dds_get_status_changes (writer, &status); 
            DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

            if (status == DDS_PUBLICATION_MATCHED_STATUS) {
                break;
            }
            /* Polling sleep. */
            dds_sleepfor (DDS_MSECS (20));
        }

        /* Create a message to write. */
        msg.userID = 1;
        msg.message = "Hello World";

        printf ("=== [Publisher]    Writing : ");
        printf ("Message (%d, %s)\n", msg.userID, msg.message);

        ret = dds_write (writer, &msg);
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Deleting the participant will delete all its children recursively as well. */
        ret = dds_delete (participant);
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        return EXIT_SUCCESS;
    }

We are using the DDS API and the ``HelloWorldData_Msg`` type to send
data, therefore, we need to include the appropriate header files as we
did in the subscriber code.


.. code-block:: C

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"

Like the reader in ``subscriber.c``, we need a participant and a
topic to create a writer. We must also use the same topic name specified in ``subscriber.c``.

.. code-block:: C

    dds_entity_t participant; 
    dds_entity_t topic; 
    dds_entity_t writer;

    participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL); 
    topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
    "HelloWorldData_Msg", NULL, NULL); 
    writer = dds_create_writer (participant, topic, NULL, NULL);

When readers and writers are sharing the same data type and topic name,
it connects them without the application's involvement.
A rendezvous pattern is required to write data only
when a DataReader appears. Such a pattern can be implemented by
either:

*  Waiting for the publication/subscription matched events, where the
   Publisher waits and blocks the writing thread until the appropriate
   publication-matched event is raised, or
*  Regularly polls the publication matching status. This is
   the preferred option we implement in this example. The following line of
   code instructs |var-project-short| to listen on the
   DDS\_PUBLICATION\_MATCHED\_STATUS:

.. code-block:: C

    dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);

At regular intervals, we get the status change and a matching
publication. In between, the writing thread sleeps for a time period
equal ``DDS\_MSECS`` (in milliseconds).

.. code-block:: C

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

After this loop, we are sure that a matching reader has been discovered.
Now, we commence the writing of the data instance. First, the data must
be created and initialized.


.. code-block:: C

    HelloWorldData_Msg msg;

    msg.userID = 1;
    msg.message = "Hello World";

Then we can send the data instance of the keyed message.

.. code-block:: C

    ret = dds_write (writer, &msg);

When terminating the program, we free the DDS allocated resources by
deleting the root entity of all the others: the domain participant.

.. code-block:: C

    ret = dds_delete (participant);

All the underlying entities, such as topic, writer, etc., are deleted.
