
The ``Publisher.c`` contains the source that writes a **Hello World!**
Message.

The DDS publisher application code is almost symmetric to the subscriber, except 
that you must create a data writer instead of a data reader. To ensure data is 
written only when at least one matching reader is discovered, a synchronization
statement is added to the main thread. Synchronizing the main thread until a reader 
is discovered ensures we can start the publisher or subscriber program in any order.

The following is a copy of the **publisher.c** file that is available from the 
|url::helloworld_c_github| repository.

.. literalinclude:: ../../../../examples/helloworld/publisher.c
    :linenos:
    
To create a publisher:

#.  Send data using the DDS API and the ``HelloWorldData_Msg`` type, include the 
    appropriate header files:

    .. code-block:: C
        :linenos:
        :lineno-start: 1

        #include "ddsc/dds.h"
        #include "HelloWorldData.h"

#.  Create a writer. You must have a participant and a topic (must have the 
    same topic name as specified in ``subscriber.c``):

    .. code-block:: C
        :linenos:
        :lineno-start: 8

        dds_entity_t participant; 
        dds_entity_t topic; 
        dds_entity_t writer;

#.  Create a Participant.

    .. code-block:: C
        :linenos:
        :lineno-start: 18

        participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL); 

#.  Create a Topic.
  
    .. code-block:: C
        :linenos:
        :lineno-start: 23

        topic = dds_create_topic (participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);

#.  Create a Writer.

    .. code-block:: C
        :linenos:
        :lineno-start: 28

        writer = dds_create_writer (participant, topic, NULL, NULL);

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
    DDS\_PUBLICATION\_MATCHED\_STATUS:

    .. code-block:: C
        :linenos:
        :lineno-start: 36

        dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);

#.  At regular intervals, the status change and a matching publication is received. In between, 
    the writing thread sleeps for a time period equal ``DDS\_MSECS`` (in milliseconds).

    .. code-block:: C
        :linenos:
        :lineno-start: 40

        while(!(status & DDS_PUBLICATION_MATCHED_STATUS))
        {
            rc = dds_get_status_changes (writer, &status);
            if (rc != DDS_RETCODE_OK)
                DDS_FATAL("dds_get_status_changes: %s\n", dds_strretcode(-rc));

            /* Polling sleep. */
            dds_sleepfor (DDS_MSECS (20));
        }

    After this loop, a matching reader has been discovered.

#.  To write the data instance, create and initialize the data:

    .. code-block:: C
        :linenos:
        :lineno-start: 12

        HelloWorldData_Msg msg;

    .. code-block:: C
        :linenos:
        :lineno-start: 51

        msg.userID = 1;
        msg.message = "Hello World";

#.  Send the data instance of the keyed message:

    .. code-block:: C
        :linenos:
        :lineno-start: 58

        rc = dds_write (writer, &msg);

#.  When terminating the program, free the DDS allocated resources by deleting the 
    root entity of all the others (the domain participant):

    .. code-block:: C
        :linenos:
        :lineno-start: 63

        rc = dds_delete (participant);

    All the underlying entities, such as topic, writer, and so on, are deleted.
